cmake_minimum_required(VERSION 3.28.3)
project(webserver1)
add_executable(${PROJECT_NAME} main.c RequestHandler.c RouteHandlers.c RouteTable.c ServerOperations.c ThreadPool.c)