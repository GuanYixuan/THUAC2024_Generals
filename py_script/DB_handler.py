import sqlite3
import datetime

from dataclasses import dataclass
from typing import Optional, Tuple, List, Dict

class AI:
    user_name: str
    ai_name: str
    version: int
    token: str
    comment: str

    def __init__(self, user_name: str, ai_name: str, version: int, token: str = "", comment: str = "") -> None:
        self.user_name = user_name
        self.ai_name = ai_name
        self.version = version
        self.token = token
        self.comment = comment

    def __repr__(self) -> str:
        return f"AI({self.user_name}'s {self.ai_name} v{self.version})"
    def __str__(self) -> str:
        return f"[{self.user_name}'s {self.ai_name} v{self.version}]"

    def __eq__(self, __value: object) -> bool:
        if not isinstance(__value, AI):
            return False
        return self.user_name == __value.user_name and self.ai_name == __value.ai_name and self.version == __value.version
    def __hash__(self) -> int:
        return hash((self.user_name, self.ai_name, self.version))

@dataclass
class Match:
    match_id: int
    timestamp: datetime.datetime
    player_id_0: int
    player_id_1: int
    winner: int
    end_state: str

class DB_handler:
    """数据库操作类"""

    conn: sqlite3.Connection
    """数据库连接对象"""
    cursor: sqlite3.Cursor

    def __init__(self, db_file: str) -> None:
        self.conn = sqlite3.connect(db_file)
        self.cursor = self.conn.cursor()
    def __del__(self) -> None:
        self.conn.close()

    def get_AI_id(self, ai: AI, add_if_not_exist: bool = False) -> int:
        """获取给定`AI`的id，当`add_if_not_exist`为`True`时，若不存在则创建"""
        result: Optional[Tuple[int]] = None
        if not len(ai.token):
            self.cursor.execute("SELECT id FROM AIs WHERE user_name=? AND ai_name=? AND version=?",
                                (ai.user_name, ai.ai_name, ai.version))
            all_results: List[Tuple[int]] = self.cursor.fetchall()
            result = all_results[0] if len(all_results) == 1 else None
            if len(all_results) > 1:
                raise ValueError(f"Multiple ids found with the same name and version: {[i[0] for i in all_results]}")
        else:
            self.cursor.execute("SELECT id FROM AIs WHERE user_name=? AND ai_name=? AND version=? AND token=?",
                                (ai.user_name, ai.ai_name, ai.version, ai.token.replace('-', '')))
            result = self.cursor.fetchone()

        if result is not None:
            return result[0]

        if not add_if_not_exist:
            raise ValueError(f"AI {ai.user_name} {ai.ai_name} {ai.version} not found in database")
        self.cursor.execute("INSERT INTO AIs (user_name, ai_name, version, token, comment) VALUES (?,?,?,?,?)",
                            (ai.user_name, ai.ai_name, ai.version, ai.token, ai.comment))
        self.conn.commit()

        ret = self.cursor.lastrowid
        assert ret is not None, "Failed to get id"
        return ret

    def get_AI_by_id(self, ai_id: int) -> AI:
        """根据`ai_id`获取`AI`对象"""
        self.cursor.execute("SELECT user_name, ai_name, version, token, comment FROM AIs WHERE id=?", (ai_id,))
        result: Optional[Tuple[str, str, int, str, str]] = self.cursor.fetchone()
        if result is None:
            raise ValueError(f"AI id {ai_id} not found in database")
        return AI(*result)

    def match_exists(self, match_id: int) -> bool:
        """判断给定`match_id`的对局是否存在"""
        self.cursor.execute("SELECT id FROM Matches WHERE id=?", (match_id,))
        return self.cursor.fetchone() is not None

    def add_match(self, match: Match) -> None:
        """向数据库中添加一个对局"""
        self.cursor.execute("INSERT INTO matches (id, timestamp, player_id_0, player_id_1, winner, end_state) VALUES (?,?,?,?,?,?)",
                            (match.match_id, match.timestamp, match.player_id_0, match.player_id_1, match.winner, match.end_state))
        self.conn.commit()

    def get_stats_for_pair(self, ai1: AI, ai2: AI) -> Tuple[int, int]:
        """获取两个AI的对局统计数据，返回一个元组(胜利次数, 总对局数)"""
        idx1 = self.get_AI_id(ai1)
        idx2 = self.get_AI_id(ai2)
        self.cursor.execute("SELECT COUNT(CASE WHEN winner=0 THEN 1 END), COUNT(*) FROM matches WHERE player_id_0=? AND player_id_1=?", (idx1, idx2))
        result1: Tuple[int, int] = self.cursor.fetchone()
        self.cursor.execute("SELECT COUNT(CASE WHEN winner=1 THEN 1 END), COUNT(*) FROM matches WHERE player_id_0=? AND player_id_1=?", (idx2, idx1))
        result2: Tuple[int, int] = self.cursor.fetchone()
        return (result1[0] + result2[0], result1[1] + result2[1])

    @dataclass
    class __Stats_for_AI_ret:
        opponent: AI
        win_count: int
        total_count: int
        last_match_time: datetime.datetime
    def get_stats_for_ai(self, ai: AI) -> List[__Stats_for_AI_ret]:
        """获取给定AI的对局统计数据，返回一个列表，每个元素是元组(对手AI, 胜利次数, 总对局数, 最后对局时间)"""
        idx = self.get_AI_id(ai)
        self.cursor.execute("SELECT player_id_0, player_id_1, COUNT(CASE WHEN winner=0 THEN 1 END), COUNT(*), MAX(timestamp) "
                            "FROM matches WHERE player_id_0=? OR player_id_1=? GROUP BY player_id_0, player_id_1", (idx, idx))
        results: List[Tuple[int, int, int, int, str]] = self.cursor.fetchall()

        ret: Dict[int, DB_handler.__Stats_for_AI_ret] = {}
        for result in results:
            if result[0] == result[1]: continue

            swap_side = result[1] == idx
            opponent_id = result[0] if swap_side else result[1]

            stat_obj = (result[3] - result[2], result[3]) if swap_side else (result[2], result[3])
            if opponent_id not in ret:
                ret[opponent_id] = DB_handler.__Stats_for_AI_ret(self.get_AI_by_id(opponent_id), *stat_obj,
                                                                 datetime.datetime.fromisoformat(result[4]))
            else:
                ret[opponent_id].win_count += stat_obj[0]
                ret[opponent_id].total_count += stat_obj[1]

        return [i for i in ret.values()]

    def get_stats_for_user(self, user_name: str) -> List[Tuple[AI, int, datetime.datetime]]:
        """获取给定用户名的所有AI的统计数据，返回一个列表，每个元素是元组(AI, AI总局数, 最后活跃时间)

        注意：AI自己与自己的对局仅记为一次
        """
        self.cursor.execute("SELECT id, ai_name, version, token, comment FROM AIs WHERE user_name=?", (user_name,))
        results: List[Tuple[int, str, int, str, str]] = self.cursor.fetchall()

        ret: List[Tuple[AI, int, datetime.datetime]] = []
        for result in results:
            ai_id = result[0]
            ai = AI(user_name, *result[1:])
            self.cursor.execute("SELECT COUNT(*), MAX(timestamp) FROM matches WHERE player_id_0=? OR player_id_1=?", (ai_id, ai_id))
            stats: Optional[Tuple[int, str]] = self.cursor.fetchone()
            assert stats is not None, f"Failed to get stats for AI {ai}"

            self.cursor.execute("SELECT COUNT(*) FROM matches WHERE player_id_0=? AND player_id_1=?", (ai_id, ai_id))
            dup_count: int = self.cursor.fetchone()[0]
            ret.append((ai, stats[0] - dup_count, datetime.datetime.fromisoformat(stats[1])))
        return ret
