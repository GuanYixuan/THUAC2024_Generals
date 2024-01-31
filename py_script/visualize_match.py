import math
import json
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
    def from_tuple(cls, dot: Sequence[float]) -> "Point2d":
        """用二元组构造Point对象"""
        return Point2d(dot[0], dot[1])
    def round_tuple(self) -> Tuple[int, int]:
        """返回四舍五入后的二元组"""
        return (round(self.x), round(self.y))

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
        self.image = Image.new('RGB', (self.IMAGE_SIZE, self.IMAGE_SIZE), color='white')
        self.draw = ImageDraw.Draw(self.image)

        # 绘制地形
        for x in range(self.MAP_SIZE):
            for y in range(self.MAP_SIZE):
                cell_type = int(self.cell_type_str[x*self.MAP_SIZE + y])
                self.fill_cell(Point2d(x, y), cell_type)

        # 绘制网格线
        for i in range(self.MAP_SIZE):
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

if __name__ == '__main__':
    # Generate the image file
    try:
        drawer = Board_drawer()
        with open("D:\\Special Project\\multi_agent\\coding\\4621648.json", "r") as file:
            last_round: int = 0
            action_number: int = 0
            for line in file:
                round_data = json.loads(line)
                round_number: int = round_data['Round']
                action_code: int = round_data["Action"][0]
                if last_round != round_number:
                    last_round = round_number
                    action_number = 0

                if round_number == 0:
                    drawer.cell_type_str = round_data['Cell_type']

                action_number += 1
                update_map_from_json(line)
                if action_code != 8:
                    drawer.map_matrix = map_matrix
                    drawer.draw_call()
                    drawer.image.save('D:\\Special Project\\multi_agent\\coding\\output\\r%d_a%d.png' % (round_number, action_number))

    except FileNotFoundError:
        print("File not found")