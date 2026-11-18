# GameBoy Emulator Frontend

## 📌 Описание
Веб-интерфейс для эмулятора GameBoy. Пользователь загружает ROM-файл (.gb), и игра запускается прямо в браузере.

## 🛠 Стек
- **Next.js** (React)
- **Material UI** (библиотека компонентов)
- **Tailwind CSS** (стили)
- **Jest + React Testing Library** (тесты)

## 🚀 Установка и запуск
```bash
npm install
npm run dev
```

## Тесты можно запустить по команде
```bash
npm test
```

Тесты находятся в frontend/components/station.test.txt
Тесты покрывают все необходимые случаи, конкретика отражена в комментариях файла frontend/components/station.test.txt

## Описание моей работы
С помощью v0.app я создал макет сайта
ИИ испоьлзовал Material UI (Button, Card, CircularProgress, Alert)
Сайт корректно работает, загружает игру в формате .gb и запускает в эмуляторе 