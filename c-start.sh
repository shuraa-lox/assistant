#!/bin/bash

cd /Users/apple/Desktop/Learn\ C++/assistan

g++ main.cpp -o main -I. -L. -lvosk -std=c++17 -lportaudio -lpthread

if [ $? -eq 0 ]; then
    echo "Сборка прошла успешно! Запуск..."
    ./main
else
    echo "Ошибка при компиляции."
fi
