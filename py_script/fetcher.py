import os
os.chdir(os.path.dirname(__file__))

import pycurl
import base64
import pickle
import io, json
import datetime

from DB_handler import DB_handler
from DB_handler import Match, AI

from typing import List, Dict, Tuple, Optional, Any
from typing import Iterable

json_t = Dict[str, Any]
replay_folder: str = "replays"

def strs_in_strs(target: "str | Iterable[str]", lib: "str | Iterable[str]") -> bool:
    if isinstance(target, str):
        target = (target,)
    if isinstance(lib, str):
        lib = (lib,)

    for _t in target:
        for _l in lib:
            if _t in _l:
                return True
    return False

class Fetcher:
    """资源下载器, 用于从saiblo网站上获取信息"""
    auth_key: str

    db_handler: DB_handler

    def __init__(self, db_path: str, _auth_key: str) -> None:
        """初始化下载器

        Args:
            _auth_key (str): 提供给服务器的token, 可以在排行榜处观察发送的https请求的authorization字段而获得
        """
        self.auth_key = _auth_key.replace("Bearer ", "")
        self.db_handler = DB_handler(db_path)

    def fetch_match(self, player: "str | Tuple[str, str] | List[str]", max_count: int = 10000, *,
                    result: Optional[bool] = None,
                    start_time: Optional[datetime.datetime] = None, end_time: Optional[datetime.datetime] = None,
                    no_download: bool = False, dig_into_history: bool = False) -> None:

        """下载一段时间内的与给定玩家相关的对局, 允许利用对局发起时间、最大下载数量等进一步限制范围

        支持同时指定多个限制条件以进行复合筛选

        状态不是"评测成功"的对局会被自动跳过

        Args:
            `player` (`str | Tuple[str, str]` | List[str]): 给定的玩家, 传入Tuple表示下载这两个玩家间的对局, 传入List表示下载与这些玩家相关的所有对局
            `max_count` (`int`, optional): 最大下载的对局数. 默认无限制.
            `result` (`bool`, optional): 给定的对局结果（第一个玩家胜利/失败）, 只有该结果的对局才会被下载. 默认无限制.
            `start_time` (`datetime.datetime`, optional): 给定的起始时间, 晚于该时间发起的对局才会被下载. 默认无限制.
            `end_time` (`datetime.datetime`, optional): 给定的结束时间, 早于该时间发起的对局才会被下载. 默认无限制.
            `no_download` (`bool`, optional): 是否不下载对局文件, 只记录对局信息到数据库. 默认下载对局文件.
            `dig_into_history` (`bool`, optional): 是否不在request中设置玩家名称参数, 可用于下载更加久远(最近1000条以外)的对局. 默认不开启, 但当`player`为List时会忽略此参数而自动开启.
        """
        print("Fetching starts at " + str(datetime.datetime.now()))

        search_player: Optional[str] = None
        if isinstance(player, str):
            search_player = player
        elif isinstance(player, tuple):
            search_player = player[0]

        if dig_into_history: search_player = None

        query_step: int = min(20, round(max_count * 1.2))
        query_offset: int = 0
        download_count: int = 0
        while True:
            # 进行一次查询
            print("Query: [limit: %d, offset: %d, player: %s]" % (query_step, query_offset, search_player))
            query_url = "https://api.saiblo.net/api/matches/?game=35&limit=%d&offset=%d%s" % \
                        (query_step, query_offset, "&username=%s" % search_player if search_player is not None else "")
            results: List[json_t]= json.loads(self.__call_pycurl(query_url))["results"]

            ending: bool = False
            for match in results:
                # 筛选符合条件的对局
                match_time = datetime.datetime.strptime(match["create_time"][:-6], '%a, %d %b %Y %H:%M:%S')
                if (start_time is not None) and match_time < start_time:
                    ending = True
                    break
                if (result is not None) and (match["info"][0]["rank"] == 1) != result:
                    continue
                if (end_time is not None) and match_time > end_time:
                    continue
                # 筛除评测失败/未完成的对局
                if (None in tuple(map(lambda x: x["code"], match["info"]))) or (match["state"] != "评测成功"):
                    continue

                match_players = tuple(map(lambda x: x["user"]["username"], match["info"]))
                match_ais = tuple(map(lambda x: AI(x["user"]["username"], x["code"]["entity"], x["code"]["version"], x["code"]["id"], x["code"]["remark"]), match["info"]))

                if isinstance(player, str): # player在match_players中
                    if not (player in match_players): continue
                elif isinstance(player, tuple): # 两个玩家都在match_players中
                    if not all(map(lambda x: x in match_players, player)): continue
                elif isinstance(player, list): # 任意一个玩家在match_players中
                    if not any(map(lambda x: x in match_players, player)): continue

                # 若不存在此对局，则创建并下载replay
                match_id: int = match["id"]
                download_dir = os.path.join(replay_folder, str(match_id))

                # 如果需要下载
                if not no_download and not os.path.exists(download_dir):
                    print("Download: %s to '%s'" % (match_id, download_dir))
                    if not os.path.exists(download_dir): os.makedirs(download_dir)

                    # 对局meta信息
                    meta_json = json.loads(self.__call_pycurl("https://api.saiblo.net/api/matches/%s/" % match_id))
                    # 将message中的独立json内容补充至meta_json中
                    msg_json: json_t = json.loads(meta_json["message"])
                    for state in msg_json["states"]:
                        state["stderr"] = base64.b64decode(state["stderr"]).decode()
                    msg_json["stdinRecords"] = [base64.b64decode(x).decode() for x in msg_json["stdinRecords"]]

                    del meta_json["message"]
                    for key, value in msg_json.items():
                        meta_json[key] = value
                    pickle.dump(meta_json, open(os.path.join(download_dir, "meta.zip"), "wb"))

                    # replay主体
                    with open(os.path.join(download_dir, "replay.json"), "w") as f:
                        f.write(self.__call_pycurl("https://api.saiblo.net/api/matches/%s/download/" % match_id))

                if not self.db_handler.match_exists(match_id):
                    print("Fetch %s: %s vs %s at %s" % (match_id, str(match_ais[0]), str(match_ais[1]), match_time))

                    # 判定end_state
                    end_state: str = match["info"][0]["end_state"]
                    p1_state: str = match["info"][1]["end_state"]
                    if end_state == "OK" and p1_state != "OK":
                        end_state = p1_state
                    elif end_state != "OK" and p1_state != "OK" and end_state != p1_state:
                        raise ValueError("Invalid end_state: %s, %s" % (end_state, p1_state))

                    # 导入数据库
                    match_obj = Match(match_id, match_time,
                                      self.db_handler.get_AI_id(match_ais[0], True), self.db_handler.get_AI_id(match_ais[1], True),
                                      0 if match["info"][0]["rank"] == 1 else 1, end_state)
                    self.db_handler.add_match(match_obj)

                download_count += 1

                if download_count >= max_count:
                    ending = True
                    break

            # 时间已经早于end_time或查询结果数量不足query_step（无更多结果）
            if ending or len(results) < query_step: break
            query_offset += query_step

    def __call_pycurl(self, url: str, extra_header: List[str] = []) -> str:
        headers = [
            "authority: api.saiblo.net", "accept: application/json, text/plain, */*", "accept-language: zh-CN,zhq=0.9",
            "authorization: Bearer %s" % self.auth_key, "origin: https://www.saiblo.net", "referer: https://www.saiblo.net/"
        ]
        headers.extend(extra_header)

        buffer = io.BytesIO()
        c = pycurl.Curl()
        c.setopt(pycurl.URL, url)
        c.setopt(pycurl.HTTPHEADER, headers)
        c.setopt(pycurl.WRITEDATA, buffer)
        c.perform()
        c.close()

        return buffer.getvalue().decode("utf-8")

class Analyzer:
    """对局分析工具, 目前用于筛选满足指定条件的对局"""
    @staticmethod
    def get_files() -> List[str]:
        return list(map(lambda x: os.path.join(replay_folder, x), os.listdir(replay_folder)))
    @staticmethod
    def clear_replay() -> None:
        """清除已下载的所有回放"""
        tuple(map(lambda file: os.remove(file), Analyzer.get_files()))
    @staticmethod
    def find_breakthrough(player: str, delete: int = 0) -> None:
        """在已下载的回放中寻找指定玩家“扣了血”的对局

        Args:
            player (str): 关注的玩家名
            delete (int, optional): 删除选项,1表示删除选中项,-1表示删除未选中项. 默认不删除.
        """
        for match_file in Analyzer.get_files():
            match_json: json_t = json.load(open(match_file))
            last_hp: Tuple[int, int] = match_json["replay"][-1]["round_state"]["camps"]

            matching: bool = False
            for ind, thing in enumerate(match_json["info"]):
                if (thing is None) or (thing["user"]["username"] != player):
                    continue
                if last_hp[ind] < 50:
                    matching = True
                    print("Match %d: hp%d %s at %s" % (thing["match"], last_hp[ind],
                        tuple(map(lambda x: "%s %s" % (x["user"]["username"], x["code"]["entity"]), match_json["info"])), match_json["create_time"]))
            if (not matching and delete == -1) or (matching and delete == 1):
                os.remove(match_file)
    @staticmethod
    def scan_result(player: str, ai_name: Optional[str] = None) -> None:
        for match_file in Analyzer.get_files():
            match_json: json_t = json.load(open(match_file))

            for ind, thing in enumerate(match_json["info"]):
                if (thing is None) or (thing["user"]["username"] != player):
                    continue
                if ai_name and not strs_in_strs(ai_name, thing["code"]["entity"]):
                    continue

                msg_json = json.loads(match_json["message"])
                msg = base64.b64decode(msg_json["states"][ind]["stderr"]).decode().splitlines()
                last_msg: str = ""
                for i in msg:
                    if i.find("511 HP:") >= 0:
                        last_msg = i.replace("511 ", "")

                print("Match %d posi%d: %s [%s] at %s" % (thing["match"], ind,
                    tuple(map(lambda x: "%s %s" % (x["user"]["username"], x["code"]["entity"]), match_json["info"])), last_msg, match_json["create_time"]))
