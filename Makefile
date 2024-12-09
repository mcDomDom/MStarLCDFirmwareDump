NAME		= MStarLCDFirmwareDump
SRCS		= MStarLCDFirmwareDump.cpp stdafx.cpp
OBJS		= $(SRCS:.cpp=.o)
CXX			= g++
#CXXFLAGS	= -Wall -Wextra -Werror

.PHONY: all
all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS)

.PHONY: clean
clean:
	$(RM) $(OBJS)

.PHONY: fclean
fclean: clean
	$(RM) $(NAME)

.PHONY: re
re: fclean all
