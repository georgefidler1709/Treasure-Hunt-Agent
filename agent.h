#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h> 

#define AGENT_START 80
#define MAX_MAP 80
#define DOUBLE_MAX_MAP 160
#define POS_AGENT_IN_VIEW 2
#define NUM_TO_NEW_LANE 5
#define MAX_DIST (DOUBLE_MAX_MAP * DOUBLE_MAX_MAP)
#define NUM_DIRECTIONS 4

#define UP 0
#define RIGHT 1
#define DOWN 2
#define LEFT 3

#define TO_HOME_PORT 0
#define WATER_CROSSING_TO_ISLAND 1

#define UNSEEN 1
#define UNSEEN_WATER 2
#define PERIMETER 3

#define RETURN_GOLD 1
#define GOLD 2
#define TOOL 3
#define OTHER 4

#define LAND 0
#define WATER 1

int   pipe_fd;
FILE* in_stream;
FILE* out_stream;

typedef struct _coords {
  int x;
  int y;
} Coords;

//stores what we know about islands mapped using create_islandMap()
typedef struct _island {
  Coords firstSpaceSeen;
  bool returnSquare;
  bool gold;
  bool tree;
  bool axe;
  bool key;
  int stones;
} Island;

//stores the path agent will take to get to the chosen island
typedef struct _island_crossing {
  int stage;
  int islandNum;
  Coords homePort;
  Coords islandPort;
  int stonesNeeded;
  bool raftNeeded;
} IslandCrossing;

//stores the information about the agent's current state
typedef struct _agent {
  Coords location;
  int direction;
  int stones;
  bool axe;
  bool key;
  bool raft;
  bool gold;
  bool currentlyRafting;
  Coords seenGold;
  Coords seenAxe;
  Coords seenKey;
} Agent;

typedef struct _searchNode *Link;

//nodes used for the priority stack for A* search and BFS
typedef struct _searchNode {
  int xPos;
  int yPos;
  int dir;
  int distFromStart;
  int heuristicCost;
  Link next;
} searchNode;


typedef struct StackRep {
  Link  top;
} StackRep;

typedef struct StackRep *Stack;

void update_agent(char action);
void update_Wmap(char view[5][5]);
void adjust_dir_vector(int *x, int *y);
char choose_action(char view[5][5], char upAction, char downAction);
int is_impassable(char space);
int adjusted_manhattan_dist(int x1, int y1, int x2, int y2, int dir);
void result_of_move(int xPos, int yPos, int dir,
  int *resultingX, int *resultingY);
int adjacent_to_impassable(int yCoord, int xCoord);
int create_islandMap(Coords agentLocation,
  int islandMap[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP],
  Island islandsInfo[40]);
bool find_island(Coords agentLocation,
  int islandMap[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP],
  int *islandX, int *islandY);
int adjacent_to(int yCoord, int xCoord, char target);
int out_of_bounds(int x, int y, int max);
int is_impassable_island_time(char space);
void choose_island(int numIslands, Island islandsInfo[40],
  IslandCrossing *ic, int islandMap[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP]);
int path_to_best_port(int *bestTargetPortX, int *bestTargetPortY, 
  int *bestHomePortX, int *bestHomePortY, Coords startPos,
  int stones, bool raft,
  int islandMap[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP]);
int find_blocking(Coords agentLocation, int *x, int *y,
  char pickup);
int adjacent_to_island_port(int yCoord, int xCoord,
  int portY, int portX);
void reset(int map[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP]);
char search_setup(int targetX, int targetY, bool waterSearch);
void Astar_search(Stack s, int targetX, int targetY,
  int predecessor[DOUBLE_MAX_MAP * DOUBLE_MAX_MAP],
  int costToReach[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP],
  bool waterSearch);
char map_path(int predecessor[MAX_MAP * MAX_MAP], int currX, int currY);
void pushOnto(Stack s, int xPos, int yPos, int dir,
  int distFromStart, int heuristicCost);
void popFrom(Stack s, int *xPos, int *yPos, int *dir,
  int *distFromStart, int *heuristicCost);
void disposeStack(Stack s);
Stack newStack();
static Link newNode(int xPos, int yPos, int dir,
  int distFromStart, int heuristicCost);
void disposeStack(Stack s);
static void disposeNode(Link curr);
int is_reachable(Coords agentLocation, int targetX, int targetY);
bool closest_reachable(Coords agentLocation, int *x, int *y, int type);
int non_return_island_check(int numIslands, IslandCrossing *ic,
  Island islandsInfo[40],
  int islandMap[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP], int type);
int return_island_check(int numIslands, IslandCrossing *ic,
  Island islandsInfo[40],
  int islandMap[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP], int type);
int stone_island_check(int numIslands, IslandCrossing *ic,
  Island islandsInfo[40],
  int islandMap[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP]);
char target_item(int targetX, int targetY);
bool on_same_island(int x1, int y1, int x2, int y2);
char collect_seen_items(void);
char clear_path(void);
char prepare_for_island_hop(void);
char water_crossing(void);
char cut_or_unlock(void);
int plan_water_path(int islandPortX, int islandPortY, int dir, int *foundPortX,
  int *foundPortY, int targetIsland, int stones, bool raft,
  int islandMap[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP]);
void count_stones(int predecessor[DOUBLE_MAX_MAP * DOUBLE_MAX_MAP],
  int currX, int currY, int startingX, int startingY, int *stones);
bool BFS_water_search(Stack s,
  int costToReach[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP],
  int predecessor[DOUBLE_MAX_MAP * DOUBLE_MAX_MAP],
  int *foundPortX, int *foundPortY, int targetIsland,
  int islandMap[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP]);
