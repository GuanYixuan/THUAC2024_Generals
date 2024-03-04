import sqlite3
import datetime

from dataclasses import dataclass

@dataclass
class AI:
    user_name: str
    ai_name: str
    version: int
    comment: str

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
        self.cursor.execute("SELECT id FROM AIs WHERE user_name=? AND ai_name=? AND version=?", (ai.user_name, ai.ai_name, ai.version))
        result = self.cursor.fetchone()
        if result is not None:
            return result[0]

        if not add_if_not_exist:
            raise ValueError(f"AI {ai.user_name} {ai.ai_name} {ai.version} not found in database")
        self.cursor.execute("INSERT INTO AIs (user_name, ai_name, version, comment) VALUES (?,?,?,?)",
                            (ai.user_name, ai.ai_name, ai.version, ai.comment))
        self.conn.commit()

        ret = self.cursor.lastrowid
        assert ret is not None, "Failed to get id"
        return ret

    def match_exists(self, match_id: int) -> bool:
        """判断给定`match_id`的对局是否存在"""
        self.cursor.execute("SELECT id FROM Matches WHERE id=?", (match_id,))
        return self.cursor.fetchone() is not None

    def add_match(self, match: Match) -> None:
        """向数据库中添加一个对局"""
        self.cursor.execute("INSERT INTO Matches (id, timestamp, player_id_0, player_id_1, winner, end_state) VALUES (?,?,?,?,?,?)",
                            (match.match_id, match.timestamp, match.player_id_0, match.player_id_1, match.winner, match.end_state))
        self.conn.commit()
