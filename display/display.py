import pygame
import sys
import time
import random

pygame.init()

# 屏幕设置
SCREEN_WIDTH = 1000
SCREEN_HEIGHT = 700
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

# 开场对话 - 修改为包含>前缀
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
    "Processing...",
    "> Fracture it."
]


class Star:
    def __init__(self):
        self.x = random.randint(0, SCREEN_WIDTH)
        self.y = random.randint(0, SCREEN_HEIGHT)
        self.size = random.randint(1, 3)
        self.brightness = random.randint(50, 200)
        self.pulse_speed = random.uniform(0.01, 0.05)
        self.pulse = random.uniform(0, 3.14)  # 随机初始相位

    def update(self):
        # 星星脉动效果
        self.pulse += self.pulse_speed
        self.current_brightness = int(
            self.brightness * (0.5 + 0.5 * abs(pygame.math.Vector2(1, 0).rotate(self.pulse * 20).x)))

    def draw(self, surface):
        color = (self.current_brightness, self.current_brightness, self.current_brightness)
        pygame.draw.circle(surface, color, (self.x, self.y), self.size)


def draw_background(surface):
    # 绘制星空背景
    surface.fill(BLACK)

    # 绘制微弱的网格线 - 调淡一些
    grid_color = (30, 30, 40)  # 更淡的灰色
    for i in range(0, SCREEN_WIDTH, 50):
        pygame.draw.line(surface, grid_color, (i, 0), (i, SCREEN_HEIGHT), 1)
    for i in range(0, SCREEN_HEIGHT, 50):
        pygame.draw.line(surface, grid_color, (0, i), (SCREEN_WIDTH, i), 1)


def main():
    clock = pygame.time.Clock()

    # 创建星星
    stars = [Star() for _ in range(200)]

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

    # 新状态：延迟计时器
    delay_timer = 0
    delay_active = False
    delay_complete = False

    # 新状态：当前行是否以>开头
    is_command_line = False
    # 新状态：当前行是否有...分隔
    has_ellipsis = False
    # 新状态：...前的部分
    first_part = ""
    # 新状态：...后的部分
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
