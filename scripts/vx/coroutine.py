from .basic import *
import heapq
from typing import Generator


# 任务类型：一个生成器，每次 yield 一个 float 表示等待的秒数
Task = Generator[float, None, None]

class Scheduler:
    def __init__(self) -> None:
        # 所有未结束的任务
        self.tasks: list[Task] = []
        # 需要立即被执行的任务
        self.ready: list[Task] = []  # FIFO
        # 处于等待状态的任务
        self.waiting: list[tuple[float, int, Task]] = []  # 最小堆 (deadline, id(task), task)

    def add_task(self, task: Task) -> None:
        self.tasks.append(task)
        self.ready.append(task)

    def mainloop(self) -> None:
        """主循环：执行任务，空闲时调用 slp_until"""
        while self.tasks:
            # 1. 执行所有就绪任务
            while self.ready:
                task = self.ready.pop(0)  # FIFO 获取就绪任务
                try:
                    wait_seconds = next(task)  # 任务执行到下一个 yield
                except StopIteration:
                    self.tasks.remove(task)  # 任务完成
                    continue

                deadline = round(get_now() + wait_seconds, 3)
                heapq.heappush(self.waiting, (deadline, id(task), task))  # 使用 id(task) 避免堆比较时 task 不可比较

            # 2. 直到没有需要运行的任务后，进入空闲等待
            if not self.ready and self.waiting:
                nearest_deadline = self.waiting[0][0]
                slp_until(nearest_deadline)

                # 3. 将已到期的任务移回就绪队列
                while self.waiting and self.waiting[0][0] <= get_now():  # 弹出所有到期任务
                    _, _, task = heapq.heappop(self.waiting)  # 弹出堆顶任务
                    self.ready.append(task)  # 放入就绪队列
