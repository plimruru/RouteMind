Здесь работа с расстоянием и временем в пути.

routing/
├── TravelInfo.h
├── Router.h
└── HaversineRouter.h

Логика:

Point A
   ↓
Router
   ↓
Point B
   ↓
distance + travel time

Router должен зависеть от model/Point.h и model/Enums.h.