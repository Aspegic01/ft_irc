NAME = ircserv
SRCS = main.cpp Server.cpp Client.cpp Command.cpp \
	   ServerHandlePass.cpp ServerHandleNick.cpp ServerHandleUser.cpp \
	   ServerHandlePing.cpp ServerHandlePong.cpp ServerHandleJoin.cpp \
	   ServerHandlePart.cpp ServerHandlePrivmsg.cpp ServerHandleTopic.cpp \
	   ServerHandleInvite.cpp ServerHandleKick.cpp ServerHandleMode.cpp \
	   ServerHandleQuit.cpp ServerHandleCommand.cpp
OBJS = $(SRCS:.cpp=.o)
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98
RM = rm -f

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^
	
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
	
clean:
	$(RM) $(OBJS)
fclean: clean
	$(RM) $(NAME)
re: fclean all
.PHONY: all clean fclean re