Главная директория.

planner/
├── Planner.h
├── Assignment.h
├── Scheduler.h
├── Constraints.h
├── Optimizer.h
├── Replanner.h
└── Explanation.h

*Constraints.h*

Проверяет:

skill
transport
time
shift

*Assignment.h*

Решает:

Request → Engineer

*Scheduler.h*

Решает:

время приезда
время начала
время окончания

*Optimizer.h*

Решает:

как сделать существующий Plan лучше

Это как раз место для алгоритма, который разрабатывает DS №1.

*Replanner.h*

Обрабатывает:

urgent request
cancel
engineer unavailable

*Explanation.h*

Генерирует причины:

почему назначили
почему не назначили
почему изменился маршрут