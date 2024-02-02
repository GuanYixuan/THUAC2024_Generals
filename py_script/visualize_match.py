import math
import json
import numpy as np
import pandas as pd
from PIL import Image, ImageDraw, ImageFont

from typing import Tuple, List, Dict
from typing import Sequence

import os
os.chdir(os.path.dirname(__file__))

MAP_SIZE = 15

# Initialize map matrix
map_matrix = [[{'soldiers': 0, 'owner': -1, 'special': ''} for _ in range(MAP_SIZE)] for _ in range(MAP_SIZE)]

# Update map from json line
def update_map_from_json(line):
    data = json.loads(line)
    # Reset special tags for all cells
    for row in map_matrix:
        for cell in row:
            cell['special'] = ''
    # Update cells based on the "Cells" field
    for cell in data['Cells']:
        x, y, owner, soldiers = cell[0][0], cell[0][1], cell[1], cell[2]
        map_matrix[y][x]['owner'] = owner
        map_matrix[y][x]['soldiers'] = soldiers
    # Update generals and oil fields
    for general in data['Generals']:
        x, y = general['Position'][0], general['Position'][1]
        if general['Type'] == 1:
            map_matrix[y][x]['special'] = '^'
        elif general['Type'] == 2:
            map_matrix[y][x]['special'] = '-'
        elif general['Type'] == 3:
            map_matrix[y][x]['special'] = '*'

class Point2d:
    """二维向量/二维点类"""
    x : float
    y : float

    def __init__(self, x0 : float, y0 : float) -> None:
        self.x = x0
        self.y = y0
    def __str__(self) -> str:
        return "(%.2f, %.2f)" % (self.x, self.y)
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

class Board_drawer:
    """盘面示意图绘制器"""

    MAP_SIZE: int = 15
    GRID_SIZE: int = 35
    IMAGE_SIZE: int = MAP_SIZE * GRID_SIZE

    TAG_OFFSET: Point2d = Point2d(GRID_SIZE - 10, 1)
    TEXT_OFFSET: Point2d = Point2d(3, 3)
    TEXT_COLOR: Dict[int, str] = {0: 'red', 1: 'blue', -1: 'gray'}

    image: Image.Image
    draw: ImageDraw.ImageDraw

    map_matrix: List[List[Dict[str, "int | str"]]]
    cell_type_str: str

    def draw_call(self, highlight_list: List[Point2d]) -> None:
        """根据map_matrix和cell_type_str绘制盘面示意图"""
        # 初始化图像
        self.image = Image.new('RGB', (self.IMAGE_SIZE + 5, self.IMAGE_SIZE + 5), color='white')
        self.draw = ImageDraw.Draw(self.image)

        # 绘制背景色
        for grid in highlight_list:
            self.highlight_cell(grid, 'yellow')

        # 绘制地形
        for x in range(self.MAP_SIZE):
            for y in range(self.MAP_SIZE):
                cell_type = int(self.cell_type_str[x*self.MAP_SIZE + y])
                self.fill_cell(Point2d(x, y), cell_type)

        # 绘制网格线
        for i in range(self.MAP_SIZE + 1):
            self.draw.line((0, i*self.GRID_SIZE, self.IMAGE_SIZE, i*self.GRID_SIZE), fill='black')
            self.draw.line((i*self.GRID_SIZE, 0, i*self.GRID_SIZE, self.IMAGE_SIZE), fill='black')

        # 写数字
        font = ImageFont.truetype('arial.ttf', 16)
        for x in range(self.MAP_SIZE):
            for y in range(self.MAP_SIZE):
                cell = map_matrix[y][x]

                if cell['soldiers'] > 0:
                    self.draw.text((self.grid_top_left(Point2d(x, y)) + self.TEXT_OFFSET).round_tuple(),
                                   str(cell['soldiers']),
                                   fill=self.TEXT_COLOR[cell['owner']], font=font)

                if len(cell['special']):
                    self.draw.text((self.grid_top_left(Point2d(x, y)) + self.TAG_OFFSET).round_tuple(),
                                   cell['special'],
                                   fill=self.TEXT_COLOR[cell['owner']], font=font)

    def grid_top_left(self, loc: Point2d) -> Point2d:
        """根据所给地图坐标`loc`（左下角为原点）返回其在图像中（左上角为原点）左上角的像素位置"""
        return Point2d(loc.x * self.GRID_SIZE, (self.MAP_SIZE - loc.y - 1) * self.GRID_SIZE)

    def grid_bottom_right(self, loc: Point2d) -> Point2d:
        """根据所给地图坐标`loc`（左下角为原点）返回其在图像中（左上角为原点）右下角的像素位置"""
        return Point2d((loc.x + 1) * self.GRID_SIZE, (self.MAP_SIZE - loc.y) * self.GRID_SIZE)

    def highlight_cell(self, grid_loc: Point2d, color: str) -> None:
        """高亮给定的地图格`grid_loc`"""
        self.draw.rectangle((self.grid_top_left(grid_loc).round_tuple(), self.grid_bottom_right(grid_loc).round_tuple()), fill=color, width=0)

    def fill_cell(self, grid_loc: Point2d, pattern_code: int) -> None:
        """向给定地图格`grid_loc`内填充指定图案"""
        if pattern_code == 0: return  # 平原无需填充

        x0, y0 = self.grid_top_left(grid_loc).round_tuple()

        PATTERN_COLOR = (192, 192, 192)

        # 斜线填充（表示沼泽）
        if pattern_code == 2:
            LINE_SPACING = 7
            LINE_WIDTH = 1
            for x in range(x0, x0 + self.GRID_SIZE, LINE_SPACING):
                self.draw.line([(x, y0), (x0 + self.GRID_SIZE, y0 + self.GRID_SIZE - (x - x0))], fill=PATTERN_COLOR, width=LINE_WIDTH)
            for y in range(y0, y0 + self.GRID_SIZE, LINE_SPACING):
                self.draw.line([(x0, y), (x0 + self.GRID_SIZE - (y - y0), y0 + self.GRID_SIZE)], fill=PATTERN_COLOR, width=LINE_WIDTH)
        # 点填充（表示沙漠）
        elif pattern_code == 1:
            DOT_SPACING = 7
            DOT_RADIUS = 1
            for i in range(x0 + DOT_SPACING, x0 + self.GRID_SIZE, DOT_SPACING):
                for j in range(y0 + DOT_SPACING, y0 + self.GRID_SIZE, DOT_SPACING):
                    self.draw.ellipse([(i, j), (i + DOT_RADIUS, j + DOT_RADIUS)], fill=PATTERN_COLOR, width=1)
        else:
            raise ValueError("Invalid pattern_code %d" % pattern_code)

class Match_visualizer:

    IMAGE_SIZE: Point2d = Point2d(1200, 600)
    COLOR_MAP: Dict[int, str] = {0: 'red', 1: 'blue', -1: 'black'}

    BOARD_OFFSET: Point2d = Point2d(30, 30)
    ROUND_INFO_OFFSET: Point2d = Point2d(BOARD_OFFSET.x + Board_drawer.IMAGE_SIZE + 30, 25)
    ACTION_OFFSET: List[Point2d] = \
        [Point2d(BOARD_OFFSET.x + Board_drawer.IMAGE_SIZE + 30, 100), Point2d(BOARD_OFFSET.x + Board_drawer.IMAGE_SIZE + 30, 300)]

    drawer: Board_drawer
    action_df: pd.DataFrame
    player_action_df: List[pd.DataFrame]
    important_action_df: List[pd.DataFrame]

    image: Image.Image
    draw: ImageDraw.ImageDraw

    def __init__(self, cell_type_str: str, action_df: pd.DataFrame) -> None:
        self.drawer = Board_drawer()
        self.drawer.cell_type_str = cell_type_str

        self.action_df = action_df
        self.player_action_df = [self.action_df[self.action_df['player'] == i].reset_index(drop=True) for i in range(2)]
        self.important_action_df = [df[(df["action_type"] >= 3) & (df["action_type"] <= 7)].reset_index(drop=True) for df in self.player_action_df]

    def draw_call(self, map_matrix: List[List[Dict[str, "int | str"]]], round_number: int, action_index: int) -> None:
        # 初始化图像
        self.image = Image.new('RGB', self.IMAGE_SIZE.round_tuple(), color='white')
        self.draw = ImageDraw.Draw(self.image)

        action = self.action_df[(self.action_df["round"] == round_number) & (self.action_df["action_index"] == action_index)].iloc[0]

        # 绘制盘面
        self.drawer.map_matrix = map_matrix
        self.drawer.draw_call(self.__get_highlight_list(action))
        self.image.paste(self.drawer.image, self.BOARD_OFFSET.round_tuple())

        # 绘制动作表
        self.dataframe_to_image(self.__get_action_slice(round_number, action_index, 0), self.ACTION_OFFSET[0], round_number, action_index)
        self.dataframe_to_image(self.__get_action_slice(round_number, action_index, 1), self.ACTION_OFFSET[1], round_number, action_index)

        # 绘制回合信息
        font = ImageFont.truetype('arial.ttf', 24)

        self.draw.text(self.ROUND_INFO_OFFSET.round_tuple(), "Round %d - Action %d" % (round_number, action_index),
                       fill="black", font=font)
        self.draw.text((self.ROUND_INFO_OFFSET + Point2d(0, font.getlength('hg') + 6)).round_tuple(), action["description"],
                       fill=self.COLOR_MAP[action["player"]], font=font)

    def __get_highlight_list(self, action: pd.Series) -> List[Point2d]:
        """根据动作信息返回需要高亮的格子"""
        desc: str = action["description"]
        if desc.find("(") != -1 and desc.find(")") != -1 and desc.find("(") < desc.find(")"):
            x, y = map(int, desc[desc.find("(")+1:desc.find(")")].split(","))
            return [Point2d(x, y)]
        else:
            return []

    def __get_action_slice(self, round_number: int, action_index: int, player: int) -> pd.DataFrame:
        SLICE_RADIUS = 3

        df = self.important_action_df[player]
        next_action_index: int = df.index[((df["round"] == round_number) & (df["action_index"] >= action_index)) | (df["round"] > round_number)].tolist()[0]

        return df.iloc[max(0, next_action_index - SLICE_RADIUS): min(len(df), next_action_index + SLICE_RADIUS + 1)]

    def dataframe_to_image(self, df: pd.DataFrame, origin: Point2d, round_number: int, action_index: int) -> None:
        """将一个DataFrame作为表格绘制到图像中"""

        df = df[["round", "action_index", "description", "remain_coins"]].copy()
        df["description"] = df["description"].apply(lambda x: x[x.find(":")+2:])

        font = ImageFont.truetype('arial.ttf', 16)
        line_height = font.getlength('hg') + 4

        # Calculate column widths based on the maximum width of content in each column
        column_widths = [max(font.getlength(str(df[col].iloc[i])) for i in range(len(df))) + 10 for col in df.columns]
        column_widths = [max(w, font.getlength(col) + 10) for w, col in zip(column_widths, df.columns)]
        total_width = sum(column_widths)

        origin_offset = origin + Point2d(10, 10)

        # Draw the headers
        draw_offset = Point2d.from_tuple(origin_offset)
        for i, col in enumerate(df.columns):
            self.draw.text(draw_offset.round_tuple(), col, fill='black', font=font)
            draw_offset.x += column_widths[i]

        # Draw the cell borders, text, and highlighted background for each row
        draw_offset = Point2d.from_tuple(origin_offset)
        draw_offset.y += line_height
        for index, row in enumerate(df.itertuples(index=False)):
            draw_offset.x = origin_offset.x
            r, ind = df.iloc[index]["round"], df.iloc[index]["action_index"]
            if r < round_number or (r == round_number and ind < action_index):
                self.draw.rectangle((origin_offset.x, draw_offset.y, origin_offset.x + total_width, draw_offset.y + line_height), fill='lightgray')
            elif r == round_number and ind == action_index:
                self.draw.rectangle((origin_offset.x, draw_offset.y, origin_offset.x + total_width, draw_offset.y + line_height), fill='yellow')
            for i, value in enumerate(row):
                cell_text = str(value)
                self.draw.text((draw_offset + Point2d(3, 0)).round_tuple(), cell_text, fill='black', font=font)
                self.draw.rectangle((draw_offset.round_tuple(), (draw_offset + Point2d(column_widths[i], line_height)).round_tuple()), outline='black', width=1)
                draw_offset.x += column_widths[i]
            draw_offset.y += line_height

def process_actions(file_name) -> pd.DataFrame:
    """将json文件中的动作数据转换为DataFrame

    Mainly AIGC
    """

    # Define constants for player names and action names
    PLAYER_NAMES = {-1: "System", 0: "Red", 1: "Blue"}
    GENERAL_TYPES = {1: "Main General", 2: "Sub General", 3: "Oil Field"}
    UPGRADE_TYPES = {1: "Produce", 2: "Defense", 3: "Mobility"}
    SKILL_NAMES = {1: "Rush", 2: "Strike", 3: "Command", 4: "Hold", 5: "Weaken"}
    TECH_NAMES = {1: "Mobility", 2: "Immune Swamp", 3: "Immune Desert", 4: "Unlock Weapon"}
    WEAPON_TYPES = {1: "Nuke", 2: "Enhance", 3: "Teleport", 4: "Timestop"}
    DIRECTIONS: Dict[int, Point2d] = {1: Point2d(-1, 0), 2: Point2d(1, 0), 3: Point2d(0, -1), 4: Point2d(0, 1)}

    # Initialize an empty list to store action info
    action_info_data = []

    # Initialize variables to track the current round and action index
    current_round = -1
    action_index = 0

    # Read the file and process each line
    with open(file_name, "r") as file:
        for line in file:
            data = json.loads(line)
            round_number = data["Round"]
            player = data["Player"]
            action = data["Action"]
            remain_coins = np.nan if player == -1 else data["Coins"][player]

            # Check if the round has changed to reset the action index
            if round_number != current_round:
                current_round = round_number
                action_index = 0

            # Extract action details
            action_code = action[0]
            raw_params = action[1:]

            # Initialize description based on action_code
            description = "Action description not set"  # Placeholder for actual description logic

            # Generate description based on action type
            if action_code == 1:  # Move Soldiers
                dest = Point2d(raw_params[0], raw_params[1]) + DIRECTIONS[raw_params[2]]
                description = f"{PLAYER_NAMES[player]}: Move {raw_params[3]} soldiers to ({dest.x}, {dest.y})"
            elif action_code == 2:  # Move General
                general_id = raw_params[0]
                general_info = next((gen for gen in data["Generals"] if gen["Id"] == general_id), None)
                general_type = GENERAL_TYPES[general_info["Type"]]
                description = f"{PLAYER_NAMES[player]}: Move {general_type} to ({raw_params[1]}, {raw_params[2]})"
            elif action_code == 3:  # Upgrade General
                general_id = raw_params[0]
                general_info = next((gen for gen in data["Generals"] if gen["Id"] == general_id), None)
                general_type = GENERAL_TYPES[general_info["Type"]]
                upgrade_type = UPGRADE_TYPES[raw_params[1]]
                upgrade_level = general_info["Level"][raw_params[1]-1]
                description = f"{PLAYER_NAMES[player]}: Upgrade {general_type}({general_info['Position'][0]}, {general_info['Position'][1]}): {upgrade_type}{upgrade_level}"
            elif action_code == 4:  # Use Skill
                skill_name = SKILL_NAMES[raw_params[1]]
                description = f"{PLAYER_NAMES[player]}: {skill_name} ({raw_params[2]}, {raw_params[3]})"
            elif action_code == 5:  # Upgrade Tech
                tech_name = TECH_NAMES[raw_params[0]]
                level = data["Tech_level"][player][raw_params[0]-1]
                description = f"{PLAYER_NAMES[player]}: Upgrade {tech_name} to {level}"
            elif action_code == 6:  # Use Super Weapon
                weapon_type = WEAPON_TYPES[raw_params[0]]
                description = f"{PLAYER_NAMES[player]}: {weapon_type} at ({raw_params[1]}, {raw_params[2]})"
            elif action_code == 7:  # Recruit General
                description = f"{PLAYER_NAMES[player]}: Recruit at ({raw_params[0]}, {raw_params[1]})"
            elif action_code == 8:  # Round Settlement
                description = "System: Round Settlement"
            elif action_code == 9:  # Game End
                reason = raw_params[1]
                description = f"System: Player {raw_params[0]} wins: {reason}"

            # Append action info to the list
            action_info_data.append([round_number, action_index, player, action_code, description, raw_params, remain_coins])

            # Increment action index for the next action
            action_index += 1

    # Create DataFrame
    return pd.DataFrame(action_info_data, columns=["round", "action_index", "player", "action_type", "description", "raw_params", "remain_coins"])
