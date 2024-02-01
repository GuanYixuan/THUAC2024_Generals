import math
import json
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

    def draw_call(self) -> None:
        """根据map_matrix和cell_type_str绘制盘面示意图"""
        # 初始化图像
        self.image = Image.new('RGB', (self.IMAGE_SIZE + 5, self.IMAGE_SIZE + 5), color='white')
        self.draw = ImageDraw.Draw(self.image)

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

    def fill_cell(self, grid_loc: Point2d, pattern_code: int) -> None:
        """向给定地图格`grid_loc`内填充指定图案"""
        if pattern_code == 0: return  # 平原无需填充

        x0, y0 = self.grid_top_left(grid_loc).round_tuple()

        PATTERN_COLOR = (160, 160, 160)

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

    BOARD_OFFSET: Point2d = Point2d(50, 50)
    RED_ACTION_OFFSET: Point2d = Point2d(600, 50)
    BLUE_ACTION_OFFSET: Point2d = Point2d(600, 400)

    drawer: Board_drawer
    action_df: pd.DataFrame

    image: Image.Image
    draw: ImageDraw.ImageDraw

    def __init__(self, cell_type_str: str, action_df: pd.DataFrame) -> None:
        self.drawer = Board_drawer()
        self.drawer.cell_type_str = cell_type_str

        self.action_df = action_df

    def draw_call(self, map_matrix: List[List[Dict[str, "int | str"]]], round_number: int, action_number: int) -> None:
        # 初始化图像
        self.image = Image.new('RGB', self.IMAGE_SIZE.round_tuple(), color='white')
        self.draw = ImageDraw.Draw(self.image)

        # 绘制盘面
        self.drawer.map_matrix = map_matrix
        self.drawer.draw_call()
        self.image.paste(self.drawer.image, self.BOARD_OFFSET.round_tuple())

        # 绘制动作表
        self.dataframe_to_image(self.__get_action_slice(round_number, action_number, 0), self.RED_ACTION_OFFSET, round_number, action_number)
        self.dataframe_to_image(self.__get_action_slice(round_number, action_number, 1), self.BLUE_ACTION_OFFSET, round_number, action_number)

    def __get_action_slice(self, round_number: int, action_number: int, player: int) -> pd.DataFrame:
        SLICE_RADIUS = 4
        raw_index: int = self.action_df[(self.action_df['round'] == round_number) & (self.action_df['action_num'] == action_number)].index[0]

        st_index = max(raw_index - 1, 0)
        rem_index = SLICE_RADIUS
        while st_index >= 0:
            action_code: int = self.action_df.iloc[st_index]['action_code']
            if self.action_df.iloc[st_index]['player'] == player and 3 <= action_code <= 7:
                rem_index -= 1
                if rem_index == 0:
                    break
            st_index -= 1
        st_index = max(st_index, 0)

        ed_index = min(raw_index + 1, len(self.action_df) - 1)
        rem_index = SLICE_RADIUS
        while ed_index < len(self.action_df):
            action_code: int = self.action_df.iloc[ed_index]['action_code']
            if self.action_df.iloc[ed_index]['player'] == player and 3 <= action_code <= 7:
                rem_index -= 1
                if rem_index == 0:
                    break
            ed_index += 1
        ed_index = min(ed_index, len(self.action_df) - 1)

        slice = self.action_df.iloc[st_index:ed_index+1]
        return slice[(slice['player'] == player) & (slice['action_code'] >= 3) & (slice['action_code'] <= 7)]

    def dataframe_to_image(self, df: pd.DataFrame, origin: Point2d, round_number: int, action_number: int) -> None:
        """将一个DataFrame作为表格绘制到图像中"""

        df = df[["round", "action_num", "action_name", "readable_params"]]

        font = ImageFont.truetype('arial.ttf', 16)
        line_height = font.getlength('hg') + 4

        # Calculate column widths based on the maximum width of content in each column
        column_widths = [max(font.getlength(str(df[col].iloc[i])) for i in range(len(df))) + 10 for col in df.columns]
        column_widths = [max(w, font.getlength(col) + 10) for w, col in zip(column_widths, df.columns)]
        total_width = sum(column_widths) + (len(df.columns) + 1) * 5

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
            if df.iloc[index]["round"] == round_number and df.iloc[index]["action_num"] == action_number:
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

    action_type_str: Dict[int, str] = {
        1: "Move Soldiers", 2: "Move Generals" , 3: "Upgrade General", 4: "Use Skill",
        5: "Upgrade Tech", 6: "Use Super Weapon", 7: "Recruit General", 8: "End of Round"}
    upgrade_type_str: Dict[int, str] = { 1: "Produce", 2: "Defense", 3: "Mobility" }
    skill_type_str: Dict[int, str] = { 1: "Rush", 2: "Strike", 3: "Command", 4: "Hold", 5: "Weaken" }
    tech_type_str: Dict[int, str] = { 1: "Mobility", 2: "Immune Swamp", 3: "Immune Desert", 4: "Unlock Weapon" }
    weapon_type_str: Dict[int, str] = { 1: "Nuke", 2: "Enhance", 3: "Teleport", 4: "Timestop" }

    actions = []

    with open(file_name, "r") as file:
        last_round: int = 0
        action_number: int = 0
        for line in file:
            data = json.loads(line)
            action_code: int = data["Action"][0]

            round_number: int = data["Round"]
            player: int = data["Player"]
            action_type = action_type_str[action_code]
            raw_params: List[int] = data["Action"][1:]

            if round_number != last_round:
                last_round = round_number
                action_number = 0
            action_number += 1

            if action_code == 1:
                readable_params = "Move %d soldiers to (%d, %d)" % (raw_params[3], raw_params[0], raw_params[1])
            elif action_code == 2:
                readable_params = "Move general to (%d, %d)" % (raw_params[1], raw_params[2])
            elif action_code == 3:
                general_id = raw_params[0]
                upgrade_type = upgrade_type_str[raw_params[1]]
                for general in data["Generals"]:
                    if general["Id"] == general_id:
                        x, y = general["Position"]
                        level = general["Level"][raw_params[1] - 1]
                        if general["Type"] == 3:
                            action_type = "Upgrade Oil Field"
                        break
                readable_params = f"{upgrade_type}{level} ({x}, {y})"
            elif action_code == 4:
                skill_type = skill_type_str[raw_params[1]]
                if len(raw_params) > 2:
                    x, y = raw_params[2], raw_params[3]
                    readable_params = f"{skill_type} ({x}, {y})"
                else:
                    readable_params = skill_type
            elif action_code == 5:
                tech_type = tech_type_str[raw_params[0]]
                readable_params = tech_type
            elif action_code == 6:
                weapon_type = weapon_type_str[raw_params[0]]
                x, y = raw_params[1], raw_params[2]
                readable_params = f"{weapon_type} ({x}, {y})"
            elif action_code == 7:
                x, y = raw_params
                readable_params = f"({x}, {y})"
            elif action_code == 8:
                readable_params = ""


            actions.append([round_number, action_number, player, action_code, action_type, readable_params, raw_params])

    df = pd.DataFrame(actions, columns=["round", "action_num", "player", "action_code", "action_name", "readable_params", "raw_params"])
    return df
