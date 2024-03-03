import os
os.chdir(os.path.dirname(__file__))

import pycurl
import base64
import sqlite3
import io, json
import datetime

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

    def __init__(self, _auth_key: str) -> None:
        """初始化下载器

        Args:
            _auth_key (str): 提供给服务器的token, 可以在排行榜处观察发送的https请求的authorization字段而获得
        """
        self.auth_key = _auth_key.replace("Bearer ", "")

    def fetch_match(self, player: "str | Tuple[str, str]", max_count: int = 10000, *,
                    result: Optional[bool] = None,
                    start_time: Optional[datetime.datetime] = None, end_time: Optional[datetime.datetime] = None) -> None:

        """下载一段时间内的与给定玩家相关的对局, 允许利用对局发起时间、最大下载数量等进一步限制范围

        支持同时指定多个限制条件以进行复合筛选

        状态不是"评测成功"的对局会被自动跳过

        Args:
            `player` (`str | Tuple[str, str]`): 给定的玩家, 传入tuple表示下载这两个玩家间的对局
            `max_count` (`int`, optional): 最大下载的对局数. 默认无限制.
            `result` (`bool`, optional): 给定的对局结果（第一个玩家胜利/失败）, 只有该结果的对局才会被下载. 默认无限制.
            `start_time` (`datetime.datetime`, optional): 给定的起始时间, 晚于该时间发起的对局才会被下载. 默认无限制.
            `end_time` (`datetime.datetime`, optional): 给定的结束时间, 早于该时间发起的对局才会被下载. 默认无限制.
        """
        print("Fetching starts at " + str(datetime.datetime.now()))

        search_player: str = player if isinstance(player, str) else player[0]
        another_player: str = "" if isinstance(player, str) else player[1]

        query_step: int = min(20, round(max_count * 1.2))
        query_offset: int = 0
        download_count: int = 0
        while True:
            # 进行一次查询
            print("Query: [limit: %d, offset: %d, player: %s]" % (query_step, query_offset, search_player))
            query_url = "https://api.saiblo.net/api/matches/?limit=%d&offset=%d&username=%s" % (query_step, query_offset, search_player)
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
                if (None in tuple(map(lambda x: x["code"], match["info"]))) or (match["state"] != "评测成功"):
                    continue

                match_players: Iterable[str] = tuple(map(lambda x: x["user"]["username"], match["info"]))
                match_ais: Iterable[str] = tuple(map(lambda x: "%s %s" % (x["code"]["entity"], x["code"]["version"]), match["info"]))

                if another_player and not strs_in_strs(another_player, match_players):
                    continue

                # 若不存在此文件夹，则创建并下载replay
                match_id: str = str(match["id"])
                if not match_id in os.listdir(replay_folder):
                    download_dir = os.path.join(replay_folder, match_id)
                    print("Download: %s to '%s'" % (match_id, download_dir))

                    if not os.path.exists(download_dir): os.makedirs(download_dir)

                    # 对局meta信息
                    with open(os.path.join(download_dir, "meta.json"), "w") as f:
                        meta_json = json.loads(self.__call_pycurl("https://api.saiblo.net/api/matches/%s/" % match_id))
                        # 将message中的独立json内容补充至meta_json中
                        msg_json: json_t = json.loads(meta_json["message"])
                        for state in msg_json["states"]:
                            state["stderr"] = base64.b64decode(state["stderr"]).decode()
                        msg_json["stdinRecords"] = [base64.b64decode(x).decode() for x in msg_json["stdinRecords"]]

                        del meta_json["message"]
                        for key, value in msg_json.items():
                            meta_json[key] = value

                        f.write(json.dumps(meta_json, indent=2, ensure_ascii=False))

                    # replay主体
                    with open(os.path.join(download_dir, "replay.json"), "w") as f:
                        f.write(self.__call_pycurl("https://api.saiblo.net/api/matches/%s/download/" % match_id))

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
