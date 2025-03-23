# 编译器和编译选项
CXX = g++
CXXFLAGS = -Wall -O2 -std=c++17

# 链接选项
LDFLAGS = -lpthread

# 源文件和目标文件
SRCS = PerformanceTest.cpp ThreadCache.cpp CentralCache.cpp PageCache.cpp
OBJS = $(SRCS:.cpp=.o)

# 可执行文件名称
TARGET = PerformanceTest.out

# 默认目标
all: $(TARGET)

# 链接目标文件生成可执行文件，使用 LDFLAGS
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# 编译源文件生成目标文件
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 清理生成的文件
clean:
	rm -f $(OBJS) $(TARGET)
    