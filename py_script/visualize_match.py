import numpy as np
import pandas as pd
from PIL import Image, ImageDraw, ImageFont

from typing import Tuple, List, Dict

import os
os.chdir(os.path.dirname(__file__))

from replay_parser import Game_snapshot, Point2d
from replay_parser import Replay_parser, Replay_loader

class Board_drawer:
    """盘面示意图绘制器"""

    MAP_SIZE: int = 15
    GRID_SIZE: int = 40
    IMAGE_SIZE: int = MAP_SIZE * GRID_SIZE

    TAG_OFFSET: Point2d = Point2d(GRID_SIZE - 10, 1)
    TEXT_OFFSET: Point2d = Point2d(3, 3)
    TEXT_COLOR: Dict[int, str] = {0: 'red', 1: 'blue', -1: 'gray'}

    image: Image.Image
    draw: ImageDraw.ImageDraw

    terrain: str

    def __init__(self, terrain: str) -> None:
        self.terrain = terrain

    def draw_call(self, gamestate: Game_snapshot, highlight_list: List[Point2d]) -> None:
        """根据map_matrix和terrain绘制盘面示意图"""
        # 初始化图像
        self.image = Image.new('RGB', (self.IMAGE_SIZE + 5, self.IMAGE_SIZE + 5), color='white')
        self.draw = ImageDraw.Draw(self.image)

        # 绘制背景色
        for grid in highlight_list:
            self.highlight_cell(grid, 'yellow')

        # 绘制地形
        for x in range(self.MAP_SIZE):
            for y in range(self.MAP_SIZE):
                cell_type = int(self.terrain[x*self.MAP_SIZE + y])
                self.fill_cell(Point2d(x, y), cell_type)

        # 绘制网格线
        for i in range(self.MAP_SIZE + 1):
            self.draw.line((0, i*self.GRID_SIZE, self.IMAGE_SIZE, i*self.GRID_SIZE), fill='black')
            self.draw.line((i*self.GRID_SIZE, 0, i*self.GRID_SIZE, self.IMAGE_SIZE), fill='black')

        # 写数字
        font = ImageFont.truetype('arial.ttf', 16)
        for x in range(self.MAP_SIZE):
            for y in range(self.MAP_SIZE):
                cell = gamestate.board[x][y]

                if cell.army > 0:
                    self.draw.text((self.grid_top_left(Point2d(x, y)) + self.TEXT_OFFSET).round_tuple(),
                                   str(cell.army),
                                   fill=self.TEXT_COLOR[cell.player], font=font)

        # 做特殊标记
        for x in range(self.MAP_SIZE):
            for y in range(self.MAP_SIZE):
                general = gamestate.find_general_at(Point2d(x, y))
                if general is None: continue

                tag = '*'
                if general.type == 1: tag = '$'
                elif general.type == 2: tag = '+'

                self.draw.text((self.grid_top_left(Point2d(x, y)) + self.TAG_OFFSET).round_tuple(),
                                tag,
                                fill=self.TEXT_COLOR[general.player], font=font)

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
            DOT_SPACING = 8
            DOT_RADIUS = 1
            for i in range(x0 + DOT_SPACING, x0 + self.GRID_SIZE, DOT_SPACING):
                for j in range(y0 + DOT_SPACING, y0 + self.GRID_SIZE, DOT_SPACING):
                    self.draw.ellipse([(i, j), (i + DOT_RADIUS, j + DOT_RADIUS)], fill=PATTERN_COLOR, width=1)
        else:
            raise ValueError("Invalid pattern_code %d" % pattern_code)

class Match_visualizer:

    IMAGE_SIZE: Point2d = Point2d(1300, 660)
    COLOR_MAP: Dict[int, str] = {0: 'red', 1: 'blue', -1: 'black'}

    BOARD_OFFSET: Point2d = Point2d(30, 30)
    ROUND_INFO_OFFSET: Point2d = Point2d(BOARD_OFFSET.x + Board_drawer.IMAGE_SIZE + 30, 25)
    ACTION_OFFSET: List[Point2d] = \
        [Point2d(BOARD_OFFSET.x + Board_drawer.IMAGE_SIZE + 30, 100), Point2d(BOARD_OFFSET.x + Board_drawer.IMAGE_SIZE + 30, 300)]

    loader: Replay_loader

    drawer: Board_drawer
    action_df: pd.DataFrame
    player_action_df: List[pd.DataFrame]
    important_action_df: List[pd.DataFrame]

    image: Image.Image
    draw: ImageDraw.ImageDraw

    def __init__(self, replay_dir: str, replay_id: str) -> None:
        self.loader = Replay_loader(replay_dir, replay_id)
        self.drawer = Board_drawer(self.loader.terrain)

        self.action_df = self.loader.action_info
        self.player_action_df = [self.action_df[self.action_df['player'] == i].reset_index(drop=True) for i in range(2)]
        self.important_action_df = [df[(df["action_type"] >= 3) & (df["action_type"] <= 7)].reset_index(drop=True) for df in self.player_action_df]

    def draw_all(self) -> None:
        image_folder = os.path.join(self.loader.replay_dir, self.loader.replay_id, "images")
        if not os.path.exists(image_folder): os.makedirs(image_folder)
        while True:
            self.draw_once()
            self.image.save(os.path.join(image_folder, "r_%d_a_%d.png" % (self.loader.curr_round, self.loader.curr_action_index)))
            if not self.loader.next():
                break

    def draw_once(self) -> None:
        # 初始化图像
        self.image = Image.new('RGB', self.IMAGE_SIZE.round_tuple(), color='white')
        self.draw = ImageDraw.Draw(self.image)

        state_key: Tuple[int, int] = (self.loader.curr_round, self.loader.curr_action_index)
        action = self.action_df[np.all(self.action_df[Replay_parser.STATE_KEYS] == state_key, axis=1)].iloc[0]

        # 绘制盘面
        self.drawer.draw_call(self.loader.gamestate , self.__get_highlight_list(action))
        self.image.paste(self.drawer.image, self.BOARD_OFFSET.round_tuple())

        # 绘制动作表
        self.dataframe_to_image(self.__get_action_slice(*state_key, 0), self.ACTION_OFFSET[0], *state_key)
        self.dataframe_to_image(self.__get_action_slice(*state_key, 1), self.ACTION_OFFSET[1], *state_key)

        # 绘制回合信息
        font = ImageFont.truetype('arial.ttf', 24)

        self.draw.text(self.ROUND_INFO_OFFSET.round_tuple(), "Round %d - Action %d" % state_key,
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
        future_actions = df.index[(df["round"] > round_number) | ((df["round"] == round_number) & (df["action_index"] >= action_index))]
        next_action_index: int = future_actions.tolist()[0] if len(future_actions) > 0 else len(df) - 1

        return df.iloc[max(0, next_action_index - SLICE_RADIUS): min(len(df), next_action_index + SLICE_RADIUS + 1)]

    def dataframe_to_image(self, df: pd.DataFrame, origin: Point2d, round_number: int, action_index: int) -> None:
        """将一个DataFrame作为表格绘制到图像中"""

        df = df[["round", "action_index", "description", "remain_coins"]].copy()

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
