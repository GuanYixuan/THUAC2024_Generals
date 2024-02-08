import math
import json
import pickle
import numpy as np
import pandas as pd
from enum import IntEnum
from dataclasses import dataclass

from typing import Optional, Tuple, List, Dict, Any
from typing import Sequence

import os
os.chdir(os.path.dirname(__file__))

MAP_SIZE = 15

PLAYER_MOBILITY_LEVEL: Dict[int, int] = {1: 2, 2: 5, 3: 8}
GENERAL_PRODUCTION_LEVEL: Dict[int, int] = {1: 1, 2: 2, 3: 4, 4: 6}
GENERAL_DEFENCE_LEVEL: Dict[int, int] = {1: 1, 2: 2, 3: 3}
GENERAL_MOBILITY_LEVEL: Dict[int, int] = {1: 1, 2: 2, 3: 4}
OIL_FIELD_DEFENCE_LEVEL: Dict[int, float] = {1: 1, 2: 1.5, 3: 2, 4: 3}

class Point2d:
    """二维向量/二维点类"""
    x : float
    y : float

    def __init__(self, x0 : float, y0 : float) -> None:
        self.x = x0
        self.y = y0
    def __str__(self) -> str:
        return "(%.2f, %.2f)" % (self.x, self.y)
    def __repr__(self) -> str:
        return self.__str__()
    @classmethod
    def from_tuple(cls, dot: "Sequence[float] | Point2d") -> "Point2d":
        """用二元组构造Point对象"""
        return Point2d(dot[0], dot[1])
    def round_tuple(self) -> Tuple[int, int]:
        """返回四舍五入后的二元组"""
        return (round(self.x), round(self.y))

    def __getitem__(self, index: int) -> float:
        if index == 0:
            return self.x
        elif index == 1:
            return self.y
        else:
            raise IndexError("Index out of range")
    def __len__(self) -> int:
        return 2

    def __eq__(self, other: "Point2d") -> bool:
        return self.x == other.x and self.y == other.y
    def __ne__(self, other: "Point2d") -> bool:
        return not self.__eq__(other)
    def __add__(self, other: "Point2d | float") -> "Point2d":
        if isinstance(other, Point2d):
            return Point2d(self.x + other.x, self.y + other.y)
        elif isinstance(other, (int, float)):
            return Point2d(self.x + other, self.y + other)
    def __sub__(self, other: "Point2d | float") -> "Point2d":
        if isinstance(other, Point2d):
            return Point2d(self.x - other.x, self.y - other.y)
        elif isinstance(other, (int,float)):
            return Point2d(self.x - other, self.y - other)
    def __mul__(self, num: float) -> "Point2d":
        assert isinstance(num, (int, float))
        return Point2d(self.x * num, self.y * num)
    def __truediv__(self, num: float) -> "Point2d":
        assert isinstance(num, (int, float))
        return Point2d(self.x / num, self.y / num)

    def length(self) -> float:
        """返回该二维向量长度"""
        return math.sqrt(self.x*self.x + self.y*self.y)
    def manhattan_distance(self, other: "Point2d") -> int:
        """返回该点与另一点的曼哈顿距离"""
        return round(abs(self.x - other.x) + abs(self.y - other.y))

@dataclass
class General_snapshot:
    id: int
    alive: bool
    player: int
    type: int
    position: Point2d
    level: List[int]
    skills_cd: List[int]
    skill_duration: List[int]
    remaining_moves: int

    def is_oil_field(self) -> bool:
        return self.type == 3

@dataclass
class Cell:
    type: int
    player: int
    army: int

@dataclass
class SuperWeapon:
    type: int
    player: int
    duration: int
    position: Point2d

class QualityType(IntEnum):
    PRODUCTION = 0
    DEFENCE = 1
    MOBILITY = 2

class TechType(IntEnum):
    MOBILITY = 0
    IMMUNE_SWAMP = 1
    IMMUNE_SAND = 2
    UNLOCK = 3

@dataclass
class Game_snapshot:
    """保存对局中的一个瞬间"""

    round: int # 当前游戏回合数
    action_index: int # 当前动作index

    generals: List[General_snapshot]
    coin: List[int] # 玩家的石油数

    # active_super_weapon: List[SuperWeapon] # 生效中的超级武器，尚未实现
    super_weapon_cd: List[int]
    tech_level: List[List[int]]
    # 科技等级列表，第一层对应玩家一，玩家二，第二层分别对应行动力，攀岩，免疫沼泽，超级武器

    remaining_moves: List[int]
    board: List[List[Cell]] # 游戏棋盘

    def find_general_at(self, position: Point2d) -> Optional[General_snapshot]:
        """返回位于指定位置的将军"""
        for general in self.generals:
            if general.position == position:
                return general
        return None

    def sum_soldiers(self, player: Optional[int]) -> int:
        """返回指定玩家的总士兵数"""
        if player is None:
            return np.sum([cell.army for row in self.board for cell in row])
        return np.sum([cell.army for row in self.board for cell in row if cell.player == player])

    def calc_oil_production(self, player: int) -> int:
        """计算指定玩家一回合的石油产量"""
        production: int = 0
        for oil_field in self.generals:
            if oil_field.player == player and oil_field.is_oil_field():
                production += GENERAL_PRODUCTION_LEVEL[oil_field.level[QualityType.PRODUCTION]]
        return production

    def calc_soldier_production(self, player: int) -> int:
        """计算指定玩家一回合的士兵招募量"""
        production: int = 0
        for general in self.generals:
            if general.player == player and (not general.is_oil_field()):
                production += GENERAL_PRODUCTION_LEVEL[general.level[QualityType.PRODUCTION]]
        return production

class Replay_parser:
    STATE_KEYS: List[str] = ["round", "action_index"]

    GLOBAL_INFO_COLS: List[str] = STATE_KEYS + ["cell_soldier", "cell_owner", "coin", "remaining_moves", "tech_level", "cd"]
    ACTION_INFO_COLS: List[str] = STATE_KEYS + ["player", "action_type", "description", "raw_params", "remain_coins"]
    GENERAL_INFO_COLS: List[str] = STATE_KEYS + ["id", "alive", "player", "type", "position", "level", "cd", "skill_state", "remaining_moves"]

    def reset(self) -> None:
        self.map_matrix = [[{'owner': -1, 'soldiers': 0} for j in range(MAP_SIZE)] for i in range(MAP_SIZE)]
        self.player_remaining_moves = [0, 0]
        self.general_remaining_moves = {}

    def parse_replay(self, replay_file: str, output_dir: str) -> None:
        assert os.path.exists(replay_file), f"{replay_file} does not exist"
        assert os.path.exists(output_dir), f"{output_dir} does not exist"

        # 建立输出路径
        replay_id: int = int(os.path.basename(replay_file).split(".")[0])
        output_dir = os.path.join(output_dir, str(replay_id))
        if not os.path.exists(output_dir):
            os.makedirs(output_dir)

        # 重置局部变量
        self.reset()
        global_info_data: List[List[Any]] = []
        action_info_data: List[List[Any]] = []
        general_info_data: List[List[Any]] = []

        # 解析回放文件
        with open(replay_file, "r") as file:
            current_round: int = -1
            action_index: int = 0
            for line in file:
                data: Dict[str, Any] = json.loads(line)
                round_number: int = data["Round"]
                action_type: int = data["Action"][0]

                # Check if the round has changed to reset the action index
                if round_number != current_round:
                    current_round = round_number
                    action_index = 0

                    # 第0回合保存地形数据
                    if round_number == 0:
                        pickle.Pickler(open(os.path.join(output_dir, "terrain.zip"), "wb")).dump(data["Cell_type"])

                state_key: List[int] = [round_number, action_index]

                if action_type == 9:  # Game End
                    action_info_data.append(state_key + self.process_actions(data))
                    break

                self.update_map_from_json(data)
                if action_type == 8: self.refresh_remaining_moves(data)

                # 处理动作数据
                action_info_data.append(state_key + self.process_actions(data))

                # 处理全局信息
                global_info_data.append(state_key + [
                    np.array([[cell['soldiers'] for cell in row] for row in self.map_matrix]),
                    np.array([[cell['owner'] for cell in row] for row in self.map_matrix]),
                    data["Coins"], self.player_remaining_moves.copy(),
                    data["Tech_level"], data["Weapon_cds"]
                ])

                # 处理将领数据
                for general in data["Generals"]:
                    general_info_data.append(state_key + [
                        general["Id"], general["Alive"], general["Player"], general["Type"], general["Position"],
                        general["Level"], general["Skill_cd"], general["Skill_rest"], self.general_remaining_moves[general["Id"]]
                    ])

                action_index += 1

        # 生成DataFrame并保存
        global_info_df = pd.DataFrame(global_info_data, columns=self.GLOBAL_INFO_COLS)
        global_info_df.to_pickle(os.path.join(output_dir, "global_info.zip"))

        action_info_df = pd.DataFrame(action_info_data, columns=self.ACTION_INFO_COLS)
        action_info_df.to_pickle(os.path.join(output_dir, "action_info.zip"))

        general_info_df = pd.DataFrame(general_info_data, columns=self.GENERAL_INFO_COLS)
        general_info_df.to_pickle(os.path.join(output_dir, "general_info.zip"))

    map_matrix: List[List[Dict[str, int]]]
    def update_map_from_json(self, data: Dict[str, Any]) -> None:
        """Update map from json line object

        Mainly AIGC
        """

        # Update cells based on the "Cells" field
        for cell in data['Cells']:
            x, y, owner, soldiers = cell[0][0], cell[0][1], cell[1], cell[2]
            self.map_matrix[x][y]['owner'] = owner
            self.map_matrix[x][y]['soldiers'] = soldiers

    player_remaining_moves: List[int]
    general_remaining_moves: Dict[int, int]
    def refresh_remaining_moves(self, data: Dict[str, Any]) -> None:
        """重置将领与玩家的剩余步数"""
        assert data["Action"][0] == 8  # Round Settlement

        for player in range(2):
            self.player_remaining_moves[player] = PLAYER_MOBILITY_LEVEL[data["Tech_level"][player][TechType.MOBILITY]]
        for general in data["Generals"]:
            self.general_remaining_moves[general["Id"]] = \
                -1 if general["Type"] == 3 else GENERAL_MOBILITY_LEVEL[general["Level"][QualityType.MOBILITY]]

    def process_actions(self, data: Dict[str, Any]) -> List[Any]:
        """将一个动作数据object转化为保存在DataFrame中的数据, 同时处理剩余步数

        Mainly AIGC
        """

        # Define constants for player names and action names
        GENERAL_TYPES = {1: "Main General", 2: "Sub General", 3: "Oil Field"}
        UPGRADE_TYPES = {1: "Produce", 2: "Defense", 3: "Mobility"}
        SKILL_NAMES = {1: "Rush", 2: "Strike", 3: "Command", 4: "Hold", 5: "Weaken"}
        TECH_NAMES = {1: "Mobility", 2: "Immune Swamp", 3: "Immune Desert", 4: "Unlock Weapon"}
        WEAPON_TYPES = {1: "Nuke", 2: "Enhance", 3: "Teleport", 4: "Timestop"}
        DIRECTIONS: Dict[int, Point2d] = {1: Point2d(-1, 0), 2: Point2d(1, 0), 3: Point2d(0, -1), 4: Point2d(0, 1)}

        player = data["Player"]
        action = data["Action"]
        remain_coins = np.nan if (player == -1 or action[0] == 9) else data["Coins"][player]

        # Extract action details
        action_code = action[0]
        raw_params = action[1:]

        # Initialize description based on action_code
        description = "Action description not set"  # Placeholder for actual description logic

        # Generate description based on action type
        if action_code == 1:  # Move Soldiers
            dest = Point2d(raw_params[0], raw_params[1]) + DIRECTIONS[raw_params[2]]
            description = f"Move {raw_params[3]} soldiers to ({dest.x}, {dest.y})"

            self.player_remaining_moves[player] -= 1
        elif action_code == 2:  # Move General
            general_id = raw_params[0]
            general_info = next((gen for gen in data["Generals"] if gen["Id"] == general_id), None)
            assert general_info is not None

            general_type = GENERAL_TYPES[general_info["Type"]]
            description = f"Move {general_type} to ({raw_params[1]}, {raw_params[2]})"

            self.general_remaining_moves[general_id] -= 1 # 【如果一步移动多格，则此处是错的】
        elif action_code == 3:  # Upgrade General
            general_id = raw_params[0]
            general_info = next((gen for gen in data["Generals"] if gen["Id"] == general_id), None)
            assert general_info is not None

            general_type = GENERAL_TYPES[general_info["Type"]]
            upgrade_type = UPGRADE_TYPES[raw_params[1]]
            upgrade_level = general_info["Level"][raw_params[1]-1]
            description = f"Upgrade {general_type}({general_info['Position'][0]}, {general_info['Position'][1]}): {upgrade_type}{upgrade_level}"
        elif action_code == 4:  # Use Skill
            general_id = raw_params[0]
            general_info = next((gen for gen in data["Generals"] if gen["Id"] == general_id), None)
            assert general_info is not None

            skill_type = raw_params[1]
            skill_name = SKILL_NAMES[skill_type]
            if skill_type == 1 or skill_type == 2:
                description = f"{skill_name} ({raw_params[2]}, {raw_params[3]})"
            else:
                description = f"{skill_name} ({general_info['Position'][0]}, {general_info['Position'][1]})"
        elif action_code == 5:  # Upgrade Tech
            tech_name = TECH_NAMES[raw_params[0]]
            level = data["Tech_level"][player][raw_params[0]-1]
            description = f"Upgrade {tech_name} to {level}"
        elif action_code == 6:  # Use Super Weapon
            weapon_type = WEAPON_TYPES[raw_params[0]]
            description = f"{weapon_type} at ({raw_params[1]}, {raw_params[2]})"
        elif action_code == 7:  # Recruit General
            description = f"Recruit at ({raw_params[0]}, {raw_params[1]})"

            self.general_remaining_moves[data["Generals"][-1]["Id"]] = 1 # 【副将一经出现则立即有行动力，此处存疑】
        elif action_code == 8:  # Round Settlement
            description = "System: Round Settlement"
        elif action_code == 9:  # Game End
            description = f"System: Player {player} wins: {data['Content']}"

        return [player, action_code, description, raw_params, remain_coins]

class Replay_loader:

    replay_dir: str
    replay_id: str
    replay_path: str

    curr_round: int
    curr_action_index: int
    curr_action_info: pd.Series
    gamestate: Game_snapshot

    terrain: str
    action_info: pd.DataFrame
    global_info: pd.DataFrame
    general_info: pd.DataFrame

    def __init__(self, replay_dir: str, replay_id: str) -> None:
        self.change_file(replay_id, replay_dir)

    def change_file(self, replay_id: str, replay_dir: Optional[str] = None) -> None:
        """切换（加载）回放文件夹"""
        if replay_dir is not None:
            self.replay_dir = replay_dir
        self.replay_id = replay_id
        self.replay_path = os.path.join(self.replay_dir, self.replay_id)

        # 加载数据
        self.terrain = pickle.load(open(os.path.join(self.replay_path, "terrain.zip"), "rb"))
        self.action_info = pd.read_pickle(os.path.join(self.replay_path, "action_info.zip"))
        self.global_info = pd.read_pickle(os.path.join(self.replay_path, "global_info.zip"))
        self.general_info = pd.read_pickle(os.path.join(self.replay_path, "general_info.zip"))

        # 从初始状态开始
        self.curr_round = 0
        self.curr_action_index = 0
        self.jump_to(0, 0)

    def jump_to(self, round_index: int, action_index: int) -> None:
        """跳转到指定时间点"""

        state_key = [round_index, action_index]

        global_line = self.global_info[np.all(self.global_info[Replay_parser.STATE_KEYS] == state_key, axis=1)].iloc[0]
        general_info = self.general_info[np.all(self.general_info[Replay_parser.STATE_KEYS] == state_key, axis=1)]

        self.curr_action_info = self.action_info[np.all(self.action_info[Replay_parser.STATE_KEYS] == state_key, axis=1)].iloc[0]

        # 建立将领列表
        general_list: List[General_snapshot] = []
        for ind, row in general_info.iterrows():
            general_list.append(General_snapshot(
                row["id"], row["alive"], row["player"], row["type"],
                Point2d.from_tuple(row["position"]), row["level"], row["cd"], row["skill_state"], row["remaining_moves"]
            ))

        # 建立地图
        soldier_matrix: np.ndarray = global_line["cell_soldier"]
        owner_matrix: np.ndarray = global_line["cell_owner"]
        board = [[Cell(int(self.terrain[MAP_SIZE * x + y]), owner_matrix[x][y], soldier_matrix[x][y]) for y in range(MAP_SIZE)] for x in range(MAP_SIZE)]

        self.gamestate = Game_snapshot(
            round_index, action_index,
            general_list, global_line["coin"],
            global_line["cd"], global_line["tech_level"], global_line["remaining_moves"],
            board
        )

        self.curr_round = round_index
        self.curr_action_index = action_index

    def next(self) -> bool:
        """下一个状态，返回是否有下一个状态"""

        state_key = [self.curr_round, self.curr_action_index]
        curr_index = self.action_info[np.all(self.action_info[Replay_parser.STATE_KEYS] == state_key, axis=1)].index[0]
        if curr_index == len(self.action_info) - 1:
            return False

        curr_index += 1
        if self.action_info.iloc[curr_index]["action_type"] == 9:
            return False

        self.jump_to(*self.action_info.iloc[curr_index][Replay_parser.STATE_KEYS])
        return True
