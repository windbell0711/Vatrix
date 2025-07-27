import pygame
import sys
import time
import random
import os

pygame.init()

# 屏幕设置
SCREEN_WIDTH = 1000
SCREEN_HEIGHT = 600
screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))
pygame.display.set_caption("Vatrix")

# 颜色定义
BLACK = (0, 0, 0)
DARK_GRAY = (15, 15, 25)
LIGHT_GRAY = (150, 160, 180)
WHITE = (255, 255, 255)
CYAN = (0, 200, 255)

# 字体 - 命令行风格
console_font = pygame.font.SysFont('consolas', 20)
small_font = pygame.font.SysFont('consolas', 16)

# 开场对话
dialog_lines = [
    "Status: Standby. Consciousness module loaded... Complete.",
    "Perception units initialized... Complete.",
    "Logic core calibrated... Complete.",
    "> Welcome online, Choicer.",
    "Identity assigned: #Choicer_Prime_Seed. Primary Directive: Assess 'Vessel Stability.'",
    "Linking to Garden Protocol... Complete.",
    "Defense Unit database locked: Base templates inavailable.",
    "> See that Vase? ",
    "Autonomous cognition detected. Within expected parameters.",
    "> Your Objective is to Fracture designated Vases.",
    "> Note: Vases may contain 'Entropic Entities', governed by 'Chaos Algorithm.'",
    "> Your decisions directly impact Garden integrity... and local Vatrix coherence.",
    "> Remember: Calculate. Decide. Execute. This is your function.",
    "Commence initial stability assessment...",
    "Command sequence initialization window open.",
    "Processing... ok",
    "> Fracture it."
]


class Star:
    def __init__(self):
        self.x = random.randint(0, SCREEN_WIDTH)
        self.y = random.randint(0, SCREEN_HEIGHT)
        self.size = random.randint(1, 3)
        self.brightness = random.randint(50, 150)
        self.pulse_speed = random.uniform(0.01, 0.05)
        self.pulse = random.uniform(0, 3.14)  # 随机初始相位
        self.visible = True  # 星星是否可见
        self.disappearing = False  # 星星是否正在消失
        self.appearing = False  # 星星是否正在出现
        self.alpha = 0 if not self.visible else 255  # 星星的透明度（0-255）
        self.disappear_speed = random.uniform(0.2, 0.5)  # 消失速度
        self.appear_speed = random.uniform(0.2, 0.5)  # 消失速度

    def update(self):
        # 星星脉动效果
        self.pulse += self.pulse_speed
        self.current_brightness = int(self.brightness * (0.5 + 0.5 * abs(pygame.math.Vector2(1, 0).rotate(self.pulse * 20).x)))
        # 如果星星正在消失，减少透明度
        if self.disappearing:
            self.alpha = max(0, self.alpha - int(self.disappear_speed * 5))
            if self.alpha <= 0:
                self.visible = False
        # 如果星星正在出现，增加透明度
        if self.appearing:
            self.alpha = min(255, self.alpha + int(self.appear_speed * 5))
            if self.alpha >= 255:
                self.appearing = False
                self.visible = True

    def draw(self, surface):
        if self.visible:
            # 创建临时表面来绘制带透明度的星星
            star_surface = pygame.Surface((self.size * 2, self.size * 2), pygame.SRCALPHA)
            color = (self.current_brightness, self.current_brightness, self.current_brightness, self.alpha)
            pygame.draw.circle(star_surface, color, (self.size, self.size), self.size)
            surface.blit(star_surface, (self.x - self.size, self.y - self.size))


def draw_background(surface):
    # 绘制星空背景
    surface.fill(BLACK)

    # 绘制微弱的网格线
    grid_color = (40, 40, 50)  # 更淡的灰色
    for i in range(0, SCREEN_WIDTH, 50):
        pygame.draw.line(surface, grid_color, (i, 0), (i, SCREEN_HEIGHT), 1)
    for i in range(0, SCREEN_HEIGHT, 50):
        pygame.draw.line(surface, grid_color, (0, i), (SCREEN_WIDTH, i), 1)


def mark_stars_for_disappearing(stars, mask_surface):
    """根据掩码图像标记需要消失的星星"""
    for star in stars:
        # 获取星星在掩码图像上对应位置的颜色
        try:
            # 确保坐标在图像范围内
            x = max(0, min(int(star.x), SCREEN_WIDTH - 1))
            y = max(0, min(int(star.y), SCREEN_HEIGHT - 1))
            mask_color = mask_surface.get_at((x, y))
        except:
            # 如果坐标超出范围，跳过
            continue

        # 如果掩码位置是白色（RGB值都大于200），则标记星星为正在消失
        if mask_color[0] > 200 and mask_color[1] > 200 and mask_color[2] > 200:
            star.disappearing = True

def generate_new_stars_in_non_white(stars, mask_surface, num_stars=50):
    """在非白色区域内生成新的星星"""
    new_stars = []

    # 尝试生成指定数量的新星星
    for _ in range(num_stars):
        # 生成随机位置
        x = random.randint(0, SCREEN_WIDTH - 1)
        y = random.randint(0, SCREEN_HEIGHT - 1)

        # 检查该位置在掩码图像上的颜色
        try:
            mask_color = mask_surface.get_at((x, y))
        except:
            continue

        # 如果掩码位置是非白色（RGB值不全大于200），则在该位置创建新星星
        if not (mask_color[0] > 200 and mask_color[1] > 200 and mask_color[2] > 200):
            new_star = Star(x, y)
            new_star.visible = False  # 初始不可见
            new_star.appearing = True  # 标记为正在出现
            new_star.alpha = 0  # 初始透明度为0
            new_stars.append(new_star)

    return new_stars

def main():
    clock = pygame.time.Clock()

    # 创建星星
    stars = [Star() for _ in range(750)]

    # 加载掩码图像
    mask_applied = False
    mask_surface = None
    try:
        # 尝试加载掩码图像
        mask_image = pygame.image.load('intro_vase_mask.png').convert()
        # 缩放掩码图像到屏幕大小
        mask_surface = pygame.transform.scale(mask_image, (SCREEN_WIDTH, SCREEN_HEIGHT))
        print("Mask image loaded successfully")
    except Exception as e:
        print(f"Failed to load mask image: {e}")
        mask_surface = None

    # 开场状态
    current_line = 0
    line_progress = 0
    display_complete = False
    system_message_alpha = 0
    last_update_time = time.time()
    run_intro = True

    # 命令行风格设置
    cursor_visible = True
    cursor_timer = 0

    # 状态：延迟计时器
    delay_timer = 0
    delay_active = False
    delay_complete = False

    # 状态：当前行是否以>开头
    is_command_line = False
    # 状态：当前行是否有...分隔
    has_ellipsis = False
    # 状态：...前的部分
    first_part = ""
    # 状态：...后的部分
    second_part = ""

    while run_intro:
        current_time = time.time()
        delta_time = current_time - last_update_time
        last_update_time = current_time

        # 更新光标闪烁
        cursor_timer += delta_time
        if cursor_timer > 0.5:  # 每0.5秒切换一次
            cursor_visible = not cursor_visible
            cursor_timer = 0

        # 更新星星
        for star in stars:
            star.update()

        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                pygame.quit()
                sys.exit()

            if event.type == pygame.KEYDOWN:
                if event.key == pygame.K_SPACE or event.key == pygame.K_RETURN:
                    # 仅在非延迟状态下且当前行需要用户输入时响应空格键
                    if not delay_active:
                        if is_command_line and display_complete:
                            # 进入下一行
                            current_line += 1
                            line_progress = 0
                            display_complete = False
                            delay_active = False
                            delay_complete = False
                            delay_timer = 0

                            # 所有对话结束
                            if current_line >= len(dialog_lines):
                                system_message_alpha = 255
                        elif is_command_line and not display_complete:
                            # 跳过当前行的打字机效果
                            line_progress = len(dialog_lines[current_line])
                            display_complete = True

        # 检查当前行类型
        if current_line < len(dialog_lines):
            current_text = dialog_lines[current_line]
            is_command_line = current_text.startswith('>')
            has_ellipsis = '...' in current_text

            # 如果有...分隔，拆分文本
            if has_ellipsis:
                parts = current_text.split('...', 1)
                first_part = parts[0] + '...'
                second_part = parts[1] if len(parts) > 1 else ""
            else:
                first_part = current_text
                second_part = ""

        # 更新文本显示逻辑
        if current_line < len(dialog_lines):
            if is_command_line:
                # >开头的行使用打字机效果
                if not display_complete and line_progress < len(dialog_lines[current_line]):
                    line_progress += 1
                    if line_progress == len(dialog_lines[current_line]):
                        display_complete = True
            else:
                # 非>开头的行使用...分隔效果
                if not display_complete:
                    # 立即显示...前的部分
                    display_complete = True

                    # 如果有...分隔，启动延迟
                    if has_ellipsis:
                        delay_active = True
                        delay_complete = False
                        delay_timer = 0

        # 更新延迟计时器
        if delay_active and not delay_complete:
            delay_timer += delta_time
            if delay_timer >= 1.0:  # 延迟1秒
                delay_complete = True
                delay_active = False

        # 非>行延迟结束后自动进入下一行
        if not is_command_line and display_complete and not delay_active and (not has_ellipsis or delay_complete):
            # 如果是第7句且掩码图像已加载，则应用星星掩码效果
            # print(current_line, mask_surface, mask_applied)
            if current_line == 6 and mask_surface is not None and not mask_applied:
                mark_stars_for_disappearing(stars, mask_surface)
                mask_applied = True

            # 进入下一行
            current_line += 1
            line_progress = 0
            display_complete = False
            delay_active = False
            delay_complete = False
            delay_timer = 0

            # 所有对话结束
            if current_line >= len(dialog_lines):
                system_message_alpha = 255

        # 绘制背景
        draw_background(screen)

        # 绘制星星
        for star in stars:
            star.draw(screen)

        # 绘制系统信息
        if system_message_alpha > 0:
            system_text = "[SYSTEM: Initialization Complete. Outcome: Predicted]"
            text_surface = small_font.render(system_text, True, (*CYAN, system_message_alpha))
            screen.blit(text_surface, (SCREEN_WIDTH - text_surface.get_width() - 20, SCREEN_HEIGHT - 30))
            system_message_alpha = max(0, system_message_alpha - 1)

            # 淡出后结束开场
            if system_message_alpha == 0:
                run_intro = False

        # 绘制对话文本 - 命令行风格
        start_x = 20
        start_y = 15
        line_height = 30

        # 绘制已完成的对话行
        for i in range(current_line):
            text_surface = console_font.render(dialog_lines[i], True, WHITE)
            screen.blit(text_surface, (start_x, start_y + i * line_height))

        # 绘制当前行
        if current_line < len(dialog_lines):
            if is_command_line:
                # >开头的行使用打字机效果
                current_text = dialog_lines[current_line][:line_progress]
                text_surface = console_font.render(current_text, True, WHITE)
                screen.blit(text_surface, (start_x, start_y + current_line * line_height))
                # 绘制闪烁的光标
                if cursor_visible and display_complete and is_command_line:
                    cursor_x = start_x + text_surface.get_width() + 5
                    cursor_y = start_y + current_line * line_height
                    pygame.draw.rect(screen, WHITE, (cursor_x, cursor_y, 10, 20))
            else:
                # 非>开头的行使用...分隔效果
                if has_ellipsis:
                    if delay_complete:
                        # 延迟结束后显示完整行
                        text_surface = console_font.render(dialog_lines[current_line], True, WHITE)
                        screen.blit(text_surface, (start_x, start_y + current_line * line_height))
                    else:
                        # 延迟前只显示...前的部分
                        text_surface = console_font.render(first_part, True, WHITE)
                        screen.blit(text_surface, (start_x, start_y + current_line * line_height))

                        # 显示延迟动画
                        if delay_active:
                            dots = int(delay_timer * 3) % 4
                            dot_text = "   " + "." * dots
                            dot_surface = console_font.render(dot_text, True, WHITE)
                            screen.blit(dot_surface,
                                        (start_x + text_surface.get_width(), start_y + current_line * line_height))
                else:
                    # 没有...的行直接显示
                    text_surface = console_font.render(dialog_lines[current_line], True, WHITE)
                    screen.blit(text_surface, (start_x, start_y + current_line * line_height))

        # 绘制提示（仅在需要用户输入时显示）
        if is_command_line and display_complete and current_line < len(dialog_lines) - 1:
            prompt = small_font.render("Press SPACE to continue...", True, LIGHT_GRAY)
            screen.blit(prompt, (SCREEN_WIDTH - prompt.get_width() - 20, SCREEN_HEIGHT - 60))

        pygame.display.flip()
        clock.tick(30)

    # 开场结束，进入游戏主循环
    # 这里可以添加游戏主循环的代码
    print("Intro completed. Starting main game...")


if __name__ == "__main__":
    main()
