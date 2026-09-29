# CMake generated Testfile for 
# Source directory: /home/mendoza/Downloads/LTT/RouteMind-develop
# Build directory: /home/mendoza/Downloads/LTT/RouteMind-develop/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test("planner_test" "/home/mendoza/Downloads/LTT/RouteMind-develop/build/planner_test")
set_tests_properties("planner_test" PROPERTIES  _BACKTRACE_TRIPLES "/home/mendoza/Downloads/LTT/RouteMind-develop/CMakeLists.txt;66;add_test;/home/mendoza/Downloads/LTT/RouteMind-develop/CMakeLists.txt;0;")
add_test("replanner_test" "/home/mendoza/Downloads/LTT/RouteMind-develop/build/replanner_test")
set_tests_properties("replanner_test" PROPERTIES  _BACKTRACE_TRIPLES "/home/mendoza/Downloads/LTT/RouteMind-develop/CMakeLists.txt;66;add_test;/home/mendoza/Downloads/LTT/RouteMind-develop/CMakeLists.txt;0;")
subdirs("_deps/json-build")
