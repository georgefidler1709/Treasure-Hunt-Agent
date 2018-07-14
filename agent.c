//By George Fidler
//4/5/18

/*
The general structure is as follows
1. If we need to cut or unlock, do so
IF WE ARE ON A RAFT ON THE WATER:
2. explore any water tiles which might gives us more information on the islands (within 2 spaces of an unseen space)
IF WE ARE ON LAND
3. if we have seen an item we would like to collect and we can path to that item by land, move to collect it. Order of importance: return gold to start, collect gold, collect tools 
4. if there is an unseen space on the world map that is reachable by land, move to check it out.
5. if there is any passable spaces on the perimeter of the current island we have not yet visited, do so (This ensures we maximise the information we need for island hopping before making any further decisions)
6. if we are yet to choose the best island we can path to with current resources (tree + axe and rocks) to travel to next, do so. Order of Importance: Initial island if we already have the gold, island with gold and a return route, island wih tool we don't yet have and a return route, island containing more or equal stones than it would take to get there, any island with guaranteed return, island with gold but no guaranteed return, island with tool but no guaranteed return, any island 
Guaranteed return = we can reach it with stones or we can see a tree on the island + we have an axe
IF NO ISLAND FOUND OR NO REACHABLE ISLAND
7. Attempt to find another route forward. This involves finding 'blocking items' stones, trees, or water which is blocking us from exploring further. If these exist, path to them and remove/traverse them. (we leave collecting trees or stones until this step as this can effect our ability to use these resources optimall for island hopping).
IF ISLAND FOUND
8. if we are on a raft, simply map by water to this island
9. if we do not have the stones.raft required to make the trip yet, gather them. Then head to the port (land tile adjacent to water tile) which was found to have the shortest route to the best island.
10. once at the port, travel the shortest route to the islands port through the water.

This order was chosen heuristically in part but largely because it guarantees we gather as much information as possible before doing anything risky, and then taking the action which has the least risk at any given time but still makes progress.

Algorithms:
A* Search - Used to search for a path whenever we know the specific space we are searching for e.g. collecting the gold when we have seen it, finding the path from one point to another.
The heuristic used is an adjusted version of the manhattan distance heuristic to adjust for the fact that turning 90 degrees takes up an action.
BFS - When we are searching for the path from one point to a specific island. We do not know which port on the specific island will be found first so we cannot use the manhattan distance heuristic but an optimal path is still vital for using stones for water traversal. For ths reasons BFS was selected.
Floodfill - floodfill is used a lot to determine the closest space of a specific type to the agent. Floodfilling radially outwards from the agent ensures the first one found will be the closest to the agent and if we only continue the search from seen passable spaces we re guaranteed a viable path.
Floodfill is also used to create an island map as all passable spaces adjacent to one another can be thought of as one island.

Structures used:
Stack: priority stack used for both A* and BFS.
For A* the stack is prioritised by distance from the start + adjusted manhattan distance.
For BFS the stack is only prioritised by distance from start.
agent: a struct containing information about the agent in its current state e.g. if it is holding a raft, if it has a key, if it has seen gold etc.
Wmap: used as the agent's general map of the world it has seen so far.
placesBeen: stores every space the agent has occupied
islandMap: a floodfill map of all islands the agent can see (areas of passable space separated by unknown space or water)
islandsInfo: a list of profiles (struct Island) for all the islands we have found, storing useful spaces found on the island (gold, tools, etc.)
IslandCrossing ic: a struct containing the information for the current planned hop from the current island to a new island.
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h> 
#include "pipe.h"
#include "agent.h"

char view[5][5];
char Wmap[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP] = {{0}};
Agent agent = {{AGENT_START, AGENT_START}, UP, 0, false, false,
  false, false, false, {-1, -1}, {-1, -1}, {-1, -1}};
IslandCrossing ic = {-1, -1, {-1,-1}, {-1,-1}, -1, false};
Island islandsInfo[40] = {0};
int islandMap[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP] = {0};
int placesBeen[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP] = {0};

//main style function uses the data collected on the world thus far
//and the position of the bot currently to decide which action to take next.
char get_action(char view[5][5]) {
  update_Wmap(view);
  char action = 0;

  //handles trees and doors
  action = cut_or_unlock();
  if(action > 0) {
    update_agent(action);
    return action;
  }

  int targetX = -1;
  int targetY = -1;
  int forwardX, forwardY;

  //if we are on a raft, explore all water tiles
  //which may have information on adjacent islands
  if(agent.currentlyRafting && closest_reachable(agent.location,
    &targetX, &targetY, UNSEEN_WATER)) {

    action = search_setup(targetX, targetY, WATER);
    ic.islandNum = -1;
    update_agent(action);
    return action;
  }
  while(action == 0) {
    if(Wmap[agent.location.y][agent.location.x] != '~') {
      //if we have seen a useful item which can be pathed to by land, do so
      action = collect_seen_items();
      if(action > 0) {
        update_agent(action);
        ic.islandNum = -1;
        return action; 
      }
      //if there is unseen space adjacent to a passable space, path there 
      //to explore the interior of the current island
      if(closest_reachable(agent.location, &targetX,
        &targetY, UNSEEN)) {
        action = search_setup(targetX, targetY, LAND); 
        update_agent(action);
        ic.islandNum = -1;
        return action;
      } 
      //explores the perimeter of the current island to
      //ensure we have full information for island hopping
      if(closest_reachable(agent.location, &targetX,
        &targetY, PERIMETER)) {
          action = search_setup(targetX, targetY, LAND);
          update_agent(action);
          ic.islandNum = -1;
          return action;
      }
    }
    //if we are not currently performing a planned island hop, map all seen islands
    int numIslands;
    if(ic.islandNum == -1) {
      numIslands = create_islandMap(agent.location,
        islandMap, islandsInfo);
      //if there are other islands to explore, chose the best one.
      //procedure explained in choose_island
      if(numIslands > 1) {
        choose_island(numIslands, islandsInfo, &ic, islandMap);
        if(ic.islandNum == -1) ic.stage = -1;
      } else {
        //if there are no other islands, then the agent must either need to return the gold
        //or there is an object blocking our path that we must collect to progress
        //(trees and rocks purposefully not collected until specifically mapped to to avoid
        //inefficient use in island hopping)
        if(agent.gold) continue;
        else {
          action = clear_path();
          if(action > 0) {
            update_agent(action);
            return action;
          }
        }
      }
    }
    //if on a raft we do not need to care about resources, just map to the chosen island
    if(agent.currentlyRafting) {
      action = water_crossing();
      update_agent(action);
      return action;
    //before the agent can complete an island hop it must appropriate collect resources
    //and move to the chosen port
    //(port meaning a passable space adjacent to water)
    } else if(ic.stage == TO_HOME_PORT) {
      action = prepare_for_island_hop();
      if(action > 0) {
        update_agent(action);
        return action;
      }
    //once at port, map a path through the water to the chosen port on the chosen island
    } else if(ic.stage == WATER_CROSSING_TO_ISLAND) {
      action = water_crossing();
      if(action > 0) {
        update_agent(action);
        return action;
      }
    //if there are other islands but we cannot reach them there must be an object blocking our path
    //so we must collect it to progress.
    } else {
      int maxCost = find_blocking(agent.location,
        &targetX, &targetY, 'o');
      if(maxCost <= 0 && agent.axe) {
        maxCost = find_blocking(agent.location,
          &targetX, &targetY, 'T');
      }
      if(maxCost > 0) {
        action = search_setup(targetX, targetY, LAND);
        update_agent(action);
        return action;
      }
    }
  }
  return 0;
}

//if we have mapped to a tree or door and have the appropriate tool,
//perform the appropriate action
char cut_or_unlock(void) {
  int forwardX, forwardY;
  char action = 0;
  result_of_move(agent.location.x, agent.location.y,
    agent.direction, &forwardX, &forwardY);
  switch (Wmap[forwardY][forwardX]){
    case '-':
      if(agent.key) action = 'u';
      break;
    case 'T':
      if(agent.axe) action = 'c';
  }
  return action;
}

//If there is a useful item (i.e. one we do not already have)
//on the same island as the agent, attempt to map to it (as we
//will have saved the item's coordinates in update_agent() when we saw it)
//Item priority order: gold, key, axe
char collect_seen_items(void) {
  char action = 0;
  if(agent.gold == true && 
    on_same_island(agent.location.x, agent.location.y,
      AGENT_START, AGENT_START)) {

    action = target_item(AGENT_START, AGENT_START);
  } 
  if(action == 0 && agent.seenGold.x != -1 && agent.gold == false && 
    on_same_island(agent.location.x, agent.location.y,
    agent.seenGold.x, agent.seenGold.y)) {

    action = target_item(agent.seenGold.x, agent.seenGold.y);
  }
  if(action == 0 && agent.seenKey.x != -1 && agent.key == false && 
    on_same_island(agent.location.x, agent.location.y,
      agent.seenKey.x, agent.seenKey.y)) {

    action = target_item(agent.seenKey.x, agent.seenKey.y);
  }
  if(action == 0 && agent.seenAxe.x != -1 && agent.axe == false && 
    on_same_island(agent.location.x, agent.location.y,
      agent.seenAxe.x, agent.seenAxe.y)) {

    action = target_item(agent.seenAxe.x, agent.seenAxe.y);
  }
  return action; 
}

//if the target  coords a reachable from the agent's current position, map to it.
char target_item(int targetX, int targetY) {
  if(is_reachable(agent.location, targetX, targetY)) {
      int action = search_setup(targetX, targetY, LAND);
    return action;
  } else return 0;   
}

//When we have no other actions, attempt to find what could be blocking our path.
//A space with the potential to block is one that is technically passable but we 
//do not treat it as such during normal exploration (trees, stones, water).
//For each blocking space type we attempt to find provable blocking space, and then failing
//that just any instance of that item as a precaution.
char clear_path(void) {
  int targetX, targetY;
  int result;
  char action = 0;
  //explore blocking water with a raft
  if(agent.raft) {
    result = find_blocking(agent.location,
      &targetX, &targetY, '~');
    if(result == 0) action = 'f';
    else if(result > 0) {
      action = search_setup(targetX, targetY, LAND);
    }
  }
  //cut down a blocking tree
  if(action == 0 && islandsInfo[0].tree == true && agent.axe) {
    result = find_blocking(agent.location,
      &targetX, &targetY, 'T');
    if(result <= 0) {
      result = closest_reachable(agent.location,
        &targetX, &targetY, 'T');
    }
    if(result > 0) {
      action = search_setup(targetX, targetY, LAND);
    }
  }
  //attempt to traverse blocking water with stones 
  if(action == 0 && agent.stones > 0) {
    result = find_blocking(agent.location,
      &targetX, &targetY, '~');
    if(result <= 0) {
      result = closest_reachable(agent.location,
        &targetX, &targetY, '~');
    }
    if(result < 0) {
      action = search_setup(targetX, targetY, LAND);
    }
  }
  //pick up a blocking stone
  if(action == 0 && islandsInfo[0].stones > 0) {
    result = find_blocking(agent.location,
      &targetX, &targetY, 'o');
    if(result <= 0)  {
      result = closest_reachable(agent.location,
        &targetX, &targetY, 'o');
    }
    //path to a blocking item if one is found
    if(result < 0) {
      action = search_setup(targetX, targetY, LAND);
    }
  }
  return action;
}

//Search for a space which could be blocking us from exploring further.
//A blocking space is defined as an space of the given char
//within 2 spaces of an unseen space

int find_blocking(Coords agentLocation, int *x,
  int *y, char blocker) {

  Coords spaces[DOUBLE_MAX_MAP * DOUBLE_MAX_MAP];
  int seen[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP] = {{0}};
  spaces[0].x = agentLocation.x;
  spaces[0].y = agentLocation.y;

  int counter = 1;
  int i, j;
//explore radially outwards from the agent's current position
  for(i = 0; i < counter; i++) {
    for(j = 0; j < 4; j++) {
      int dir = (agent.direction + j) % 4; 
      int forwardX, forwardY;
      result_of_move(spaces[i].x, spaces[i].y, dir,
        &forwardX, &forwardY);
      //if we have already tested this space, skip it.
      if(seen[forwardY][forwardX] == 1) continue;
      //if we find a blocker within 2 of an unseen space,
      //return it's location and it's distance away from the agent
      //to be mapped to
      if(Wmap[forwardY][forwardX] == blocker) {
        int blockerDir;
        for(blockerDir = 0; blockerDir < 4; blockerDir++) {
          int blockerX, blockerY;
          result_of_move(forwardX, forwardY, blockerDir,
            &blockerX, &blockerY);
          bool passCondition = false;
          if(blocker == '~') {
            if(Wmap[blockerY][blockerX] == '~' && 
              adjacent_to(blockerY, blockerX, 0) != -1) {
              passCondition = true;
            }
          } else if ((is_impassable_island_time(Wmap[blockerY][blockerX]) == 0) &&
            adjacent_to(blockerY, blockerX, 0) != -1) {
            passCondition = true;
          }
          if(passCondition == true) {
            *x = forwardX;
            *y = forwardY;
            return adjusted_manhattan_dist(forwardX, forwardY,
              agentLocation.x, agentLocation.y, agent.direction);
          }
        }
      }
      //otherwise if the space is passable add it to the queue of spaces to be search from.
      if(is_impassable(Wmap[forwardY][forwardX]) == 0) {
        spaces[counter].x = forwardX;
        spaces[counter].y = forwardY;
        seen[forwardY][forwardX] = 1;
        counter++;
      }
    }
  }
  return -1;
}

//Collect the appropriate items for the planned island hop and then
//move to the chosen port for the current island to begin water traversal
char prepare_for_island_hop(void) {
  int targetX, targetY;
  int forwardX, forwardY;
  int maxCost;
  char action = 0;

  //if we need more stones to complete the trip, collect them
  //prioritising rocks which may be blocking further exploration
  if(ic.stonesNeeded && ic.stonesNeeded > agent.stones) {
    maxCost = find_blocking(agent.location,
      &targetX, &targetY, 'o');
    if(maxCost <= 0) {
      maxCost = closest_reachable(agent.location,
        &targetX, &targetY, 'o');
    }
    if(maxCost > 0) {
      action = search_setup(targetX, targetY, LAND);
    }
  }
  //if we need a raft to complete the trip, collect one.
  //prioritising a tree which may be blocking further exploration
  if(action == 0 && ic.raftNeeded && !agent.raft) {
    maxCost = find_blocking(agent.location,
      &targetX, &targetY, 'T');
    if(maxCost <= 0) {
      maxCost = closest_reachable(agent.location,
        &targetX, &targetY, 'T');
    }
    if(maxCost > 0) {
      action = search_setup(targetX, targetY, LAND);
    }
  }
  //Once we have the appropriate resources move to the port
  if(action == 0) {
    maxCost = adjusted_manhattan_dist(ic.homePort.x, ic.homePort.y, 
      agent.location.x, agent.location.y, agent.direction);
    if(maxCost > 0) {
      action = search_setup(ic.homePort.x, ic.homePort.y, LAND);
    }
  } 
  //once we are at the port, change stage to signify that we have begun
  //water crossing and make the first water traversal action
  if(action == 0) {
    ic.stage = WATER_CROSSING_TO_ISLAND; 
    action = search_setup(ic.islandPort.x, ic.islandPort.y, WATER);
    result_of_move(agent.location.x, agent.location.y, 
      agent.direction, &forwardX, &forwardY);

    //if this action would put us on the new island, change islandNum to signify
    //we have finished island hopping.
    if(action == 'f' && forwardY == ic.islandPort.y &&
      forwardX == ic.islandPort.x) ic.islandNum = -1;

  } 
  return action;  
}

//Once we have reached port (or are on a raft) we can simply map
//across the water to the chosen island's closest port
char water_crossing(void) {
  int forwardX, forwardY;
  char action = 0;
  action = search_setup(ic.islandPort.x, ic.islandPort.y, WATER);
  result_of_move(agent.location.x, agent.location.y,
    agent.direction, &forwardX, &forwardY);

    //if this action would put us on the new island, change islandNum to signify
  //we have finished island hopping.
  if(action == 'f' && forwardY == ic.islandPort.y && 
    forwardX == ic.islandPort.x) {
    ic.islandNum = -1;
    ic.stage = -1;
  }
  return action;
}

//updates the players coords and direction to account for upcoming action
void update_agent(char action) {
  switch(action) {
    case 'f': case 'F':
      //how moving forward effects the agent depends on the direction it's facing.
      switch(agent.direction) {
        case UP: 
          agent.location.y--;
          break;
        case DOWN:
          agent.location.y++;
          break;
        case LEFT:
          agent.location.x--;
          break;
        case RIGHT:
          agent.location.x++;
          break;
        default:
          perror("direction out of range\n");
          exit(0);
      }
      //saving information if we have collected a new item (or used a depletable one)
      if(view[POS_AGENT_IN_VIEW -1][POS_AGENT_IN_VIEW] == '$') {
        agent.gold = true;
      }
      if(view[POS_AGENT_IN_VIEW -1][POS_AGENT_IN_VIEW] == 'a') {
        agent.axe = true;
      }
      if(view[POS_AGENT_IN_VIEW -1][POS_AGENT_IN_VIEW] == 'k') {
        agent.key = true;
      }
      if(view[POS_AGENT_IN_VIEW -1][POS_AGENT_IN_VIEW] == 'o') {
        agent.stones++;
      }
      if(view[POS_AGENT_IN_VIEW -1][POS_AGENT_IN_VIEW] == '~') {
        if(agent.stones > 0) agent.stones--;
        else  agent.currentlyRafting = true;
      }
      //save whether or not we are currently on a raft
      if(view[POS_AGENT_IN_VIEW -1][POS_AGENT_IN_VIEW]
        != '~' && agent.currentlyRafting == true) {

        agent.raft = false;
        agent.currentlyRafting = false;
      }
      break;
    //saving directional changes
    case 'r': case 'R':
      agent.direction++;
      agent.direction = agent.direction % 4;
      break;
    case 'l': case 'L':
      agent.direction--;
      if(agent.direction < 0) agent.direction = LEFT;
      break;
    case 'c': case 'C':
      if(agent.axe == true &&
        view[POS_AGENT_IN_VIEW -1][POS_AGENT_IN_VIEW] == 'T') {
        agent.raft = true;
      }
      else {
        perror("trying to cut when unable (no axe or no tree)\n");
        exit(0);
      }
  }
  return;
}

//adjusts a direction vector x,y to account for the orientation of the agent in the world map.
void adjust_dir_vector(int *x, int *y) {
  int tmp;
  //applies the effect of the appropriate rotation matrix to the vector
  switch(agent.direction) {
    case UP:
      break;
    case RIGHT:
      tmp = *x;
      *x = *y;
      *y = tmp;
      *x = -*x;
      break;
    case DOWN:
      *x = -*x;
      *y = -*y;
      break;
    case LEFT:
      tmp = *x;
      *x = *y;
      *y = tmp;
      *y = -*y;
      break;
    default:
      perror("direction out of range\n");
      exit(0);
  }
  return;
}

//adds the agent's current view to the world map
void update_Wmap(char view[5][5]) {
  int yCount, xCount;
  for(yCount = 0; yCount < 5; yCount++) {
    //as we know where the agent is on the world map we can convert what it is seeing to map coords.
    for(xCount = 0; xCount < 5; xCount++) {
      //components of the direction vector to take the agent from its current position to this space in view.
      int yVec = yCount - POS_AGENT_IN_VIEW;
      int xVec = xCount - POS_AGENT_IN_VIEW;
      //adjust the direction vector to the world map.
      adjust_dir_vector(&xVec, &yVec);
      int mapXCoord = agent.location.x + xVec;
      int mapYCoord = agent.location.y + yVec;
      //if the current view contains a useful item, save its coordinates
      switch(view[yCount][xCount]) {
        case '$':
          agent.seenGold.x = mapXCoord;
          agent.seenGold.y = mapYCoord;
          break;
        case 'a':
          agent.seenAxe.x = mapXCoord;
          agent.seenAxe.y = mapYCoord;
          break;
        case 'k':
          agent.seenKey.x = mapXCoord;
          agent.seenKey.y = mapYCoord;
          break;
      }
     
      //record which spaces the agent has visited
      //# is used to differentiate empty spaces we have seen
      //from those we havent..
      if(mapYCoord == agent.location.y &&
        mapXCoord == agent.location.x) {

        placesBeen[agent.location.y][agent.location.x] = 1;
        if(Wmap[mapYCoord][mapXCoord] == '~') {
          islandMap[mapYCoord][mapXCoord] = -1;
        } else Wmap[mapYCoord][mapXCoord] = '#';
        continue;
      } 
      else if(view[yCount][xCount] == ' ' ||
        view[yCount][xCount] == 'O') {
        Wmap[mapYCoord][mapXCoord] = '#';
      }
      else Wmap[mapYCoord][mapXCoord] = view[yCount][xCount];
    }
  }
  return;
}

void print_view()
{
  int i,j;

  printf("\n+-----+\n");
  for( i=0; i < 5; i++ ) {
    putchar('|');
    for( j=0; j < 5; j++ ) {
      if(( i == 2 )&&( j == 2 )) {
        putchar( '^' );
      }
      else {
        putchar( view[i][j] );
      }
    }
    printf("|\n");
  }
  printf("+-----+\n");
}

int main( int argc, char *argv[] )
{
  char action;
  int sd;
  int ch;
  int i,j;

  if ( argc < 3 ) {
    printf("Usage: %s -p port\n", argv[0] );
    exit(1);
  }

    // open socket to Game Engine
  sd = tcpopen("localhost", atoi( argv[2] ));

  pipe_fd    = sd;
  in_stream  = fdopen(sd,"r");
  out_stream = fdopen(sd,"w");

  while(1) {
      // scan 5-by-5 wintow around current location
    for( i=0; i < 5; i++ ) {
      for( j=0; j < 5; j++ ) {
        if( !(( i == 2 )&&( j == 2 ))) {
          ch = getc( in_stream );
          if( ch == -1 ) {
            exit(1);
          }
          view[i][j] = ch;
        }
      }
    }

    //print_view(); // COMMENT THIS OUT BEFORE SUBMISSION
    action = get_action( view );
    putc( action, out_stream );
    fflush( out_stream );
  }

  return 0;
}

//returns the square the agent would occupy if it was to move forward
//from a given square facing a given directon
void result_of_move(int xPos, int yPos, int dir,
  int *resultingX, int *resultingY) {

  switch (dir) {
    case UP:
      *resultingX = xPos;
      *resultingY = yPos - 1;
      break;
    case RIGHT:
      *resultingX = xPos + 1;
      *resultingY = yPos;
      break;
    case DOWN:
      *resultingX = xPos;
      *resultingY = yPos + 1;
      break;
    case LEFT:
      *resultingX = xPos - 1;
      *resultingY = yPos;
  }
  return;
}

//creates a map of every island seen by the agent
//done by finding a passable space and floodfilling all passable spaces around it
//each floodfilled section is it's own island. 
int create_islandMap(Coords agentLocation,
  int islandMap[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP], 
  Island islandsInfo[40]) {

  reset(islandMap);
  Coords spaces[DOUBLE_MAX_MAP * DOUBLE_MAX_MAP];
  int seen[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP] = {{0}};
  int islandCounter = 1;
  int i,dir;

  while(find_island(agentLocation, islandMap, &(spaces[0].x),
    &(spaces[0].y))) {
    //for each new island found we set up a Island struct in islandsInfo
    //with all gathered information, used to determine how valuable it
    //is to travel to this island
    islandMap[spaces[0].y][spaces[0].x] = islandCounter;
    int islandIdx = islandCounter - 1;
    islandsInfo[islandIdx].firstSpaceSeen.x = spaces[0].x;
    islandsInfo[islandIdx].firstSpaceSeen.y = spaces[0].y;
    islandsInfo[islandIdx].returnSquare = false;
    islandsInfo[islandIdx].gold = false;
    islandsInfo[islandIdx].tree = false;
    islandsInfo[islandIdx].key = false;
    islandsInfo[islandIdx].axe = false;
    islandsInfo[islandIdx].stones = 0;
    seen[spaces[0].y][spaces[0].x] = 1;

    if(Wmap[spaces[0].y][spaces[0].x] == '~') {
      islandCounter++;
      continue;
    }
    int i,j;
    int counter = 1;
    //we flood fill to search every passable square to collect all important information
    //about the island (shape, items etc.)
    for(i = 0; i < counter; i++) {
      if(spaces[i].y == AGENT_START && spaces[i].x == AGENT_START) {
        islandsInfo[islandIdx].returnSquare = true;
      }
      switch (Wmap[spaces[i].y][spaces[i].x]) {
        case '$':
          islandsInfo[islandIdx].gold = true;
          break;
        case 'T':
          islandsInfo[islandIdx].tree = true;
          break;
        case 'k':
          islandsInfo[islandIdx].key = true;
          break;
        case 'a':
          islandsInfo[islandIdx].axe = true;
          break;
        case 'o':
          islandsInfo[islandIdx].stones++;
      }
      for(j = 0; j < 4; j++) {
        int dir = (agent.direction + j) % 4; 
        int forwardX, forwardY;
        result_of_move(spaces[i].x, spaces[i].y, dir,
          &forwardX, &forwardY);
        if(seen[forwardY][forwardX] == 1) continue;
        if(is_impassable_island_time(Wmap[forwardY][forwardX]) == 0) {
          spaces[counter].x = forwardX;
          spaces[counter].y = forwardY;
          islandMap[forwardY][forwardX] = islandCounter;
          seen[forwardY][forwardX] = 1;
          counter++;
        }
      }
    }
    islandCounter++;
  }
  return islandCounter - 1;
}

//scans outwards radially from the agent to find passable spaces which are not adjacent
//to any other island i.e. a new island
//we then floodfill from this space in creat_islandMap()
bool find_island(Coords agentLocation,
  int islandMap[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP], 
  int *islandX, int *islandY) {

  if(islandMap[agentLocation.y][agentLocation.x] == 0) {
    *islandX = agentLocation.x;
    *islandY = agentLocation.y;
    return 1;
  }

  Coords spaces[DOUBLE_MAX_MAP * DOUBLE_MAX_MAP];
  int seen[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP] = {{0}};
  spaces[0].x = agentLocation.x;
  spaces[0].y = agentLocation.y;
  seen[spaces[0].y][spaces[0].x] = 1;
  int i,j;
  int counter = 1;
  //scans radially outwards from the agent
  for(i = 0; i < counter; i++) {
    for(j = 0; j < 4; j++) {
      int dir = (agent.direction + j) % 4; 
      int forwardX, forwardY;
      result_of_move(spaces[i].x, spaces[i].y, dir,
        &forwardX, &forwardY);
      if(out_of_bounds(forwardX, forwardY, DOUBLE_MAX_MAP)) continue;
      //stones and trees (if we have an axe) are treated as passable so we do not miss an island
      if(is_impassable_island_time(Wmap[forwardY][forwardX]) == false &&
        islandMap[forwardY][forwardX] == 0 && 
        adjacent_to(forwardY, forwardX, '~') != -1) {
        //new island must have a space adjacent to water, this stops an unreachable island
        //(e.g. surrounded by stones) from being considered
        *islandX = forwardX;
        *islandY = forwardY;
        return 1; 
      }
      if(seen[forwardY][forwardX] == 1 ||
        Wmap[forwardY][forwardX] == 0) continue;
      else {
        spaces[counter].x = forwardX;
        spaces[counter].y = forwardY;
        seen[forwardY][forwardX] = 1;
        counter++;
      }
    }
  }
  return 0;
}

//given a list of islands discovered during create_islandMap()
//choose which one we want to travel to first
//Order of importance is largely heuristic
//All islands must be reachable with the resources on the agent's current island to be chosen.
//1. Initial island if we already have the gold
//2. island with gold and a return route
//3. island wih tool we don't yet have and a return route
//4. island containing more or equal stones than it would take to get there
//5. any island with guaranteed return
//6. island with gold but no guaranteed return
//7. island with tool but no guaranteed return
//8. any island 
//an island with guaranteed return is defined as being reachable using stones or containing a tree (if we have an axe)
void choose_island(int numIslands, Island islandsInfo[40],
  IslandCrossing *ic, int islandMap[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP]) {

  int targetX;
  int targetY;
  ic->stage = TO_HOME_PORT;
  ic->homePort.x = -1;
  ic->homePort.y = -1;
  ic->islandPort.x = -1;
  ic->islandPort.y = -1;
  int stonesNeeded = 0;
  int raftNeeded = false;
  //no point picking an island if there is only 1
  if(numIslands < 2) return;
  //1. Initial island if we already have the gold
  if(non_return_island_check(numIslands, ic, islandsInfo, islandMap,
    RETURN_GOLD)) return;
  //2. island with gold and a return route
  if(return_island_check(numIslands, ic, islandsInfo, islandMap,
    GOLD)) return;
  //3. island wih tool we don't yet have and a return route
  if(return_island_check(numIslands, ic, islandsInfo, islandMap, 
    TOOL)) return;
  //4. island containing more or equal stones than it would take to get there
  if(stone_island_check(numIslands, ic, islandsInfo, islandMap)) {
    return;
  }
  //5. any island with guaranteed return
  if(return_island_check(numIslands, ic, islandsInfo, islandMap, 
    OTHER)) return;
  //6. island with gold but no guaranteed return
  if(non_return_island_check(numIslands, ic, islandsInfo, islandMap,
    GOLD)) return;
  //7. island with tool but no guaranteed return
  if(non_return_island_check(numIslands, ic, islandsInfo, islandMap,
    TOOL)) return;
  //8. any island
  if(non_return_island_check(numIslands, ic, islandsInfo, islandMap,
    OTHER)) return;
}

//Check if an island with the current requirement (type) exists and if it is reachable.
//if so save this island and the path to this island as our current IslandCrossing.
int non_return_island_check(int numIslands, IslandCrossing *ic,
  Island islandsInfo[40],
  int islandMap[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP], int type) {

  int targetIsland;
  int targetX, targetY;
  //check to see if any island has the given requirement (type)
  for(targetIsland = 1; targetIsland < numIslands; targetIsland++) {
    int caseSwitch = false;
    switch (type) {
      case RETURN_GOLD:
        if(islandsInfo[targetIsland].returnSquare && agent.gold) {
          caseSwitch = true;
        }
        break;

      case GOLD:
        if(islandsInfo[targetIsland].gold &&
          closest_reachable(islandsInfo[targetIsland].firstSpaceSeen,
          &targetX, &targetY, UNSEEN) != -1) caseSwitch = true;
        break;

      case TOOL:
        if(((islandsInfo[targetIsland].axe && !agent.axe) ||
          (islandsInfo[targetIsland].key && !agent.key)) && 
          closest_reachable(islandsInfo[targetIsland].firstSpaceSeen,
          &targetX, &targetY, UNSEEN) != -1) caseSwitch = true;

      default: 
        caseSwitch = true;
    }
    //if we found the wanted tool at the current island, check to see if we can path
    //by water to this island (using the shortest possible path)
    if(caseSwitch == true) {
      if(targetIsland != numIslands) {
        int targetX, targetY;
        if((agent.raft || (islandsInfo[0].tree && agent.axe))) {
          path_to_best_port(&(ic->islandPort.x), &(ic->islandPort.y),
            &(ic->homePort.x), &(ic->homePort.y),
            islandsInfo[targetIsland].firstSpaceSeen,
            islandsInfo[0].stones + agent.stones, 1, islandMap);
          //if we can reach the island, save the method we took to get there
          //and each end of the path (i.e. the port on the current island and the new island)
          if(ic->homePort.x != -1) {
            ic->raftNeeded = true;
            ic->stonesNeeded = 0;
            ic->islandNum = targetIsland + 1;
            return 1;
          }
        //returning gold is an edge case as we do not care about if we can return or not when using a raft
        //but we will not if it is reachable by stones previously in return_island_check(), so we must test this.
        } else if (type == RETURN_GOLD) {
          if(islandsInfo[0].stones + agent.stones > 0) {
            ic->stonesNeeded = path_to_best_port(&(ic->islandPort.x),
              &(ic->islandPort.y), &(ic->homePort.x), &(ic->homePort.y),
              islandsInfo[targetIsland].firstSpaceSeen,
              islandsInfo[0].stones + agent.stones, 0, islandMap);

            if(ic->homePort.x != -1) {
              ic->raftNeeded = false;
              ic->islandNum = targetIsland + 1;
              return 1; 
            } 
          }
        }
      }
    }
  }
  return 0;
}

//Check if an island with the current requirement (type) exists, if it is reachable and it has a guaranteed return.
//if so save this island and the path to this island as our current IslandCrossing.
int return_island_check(int numIslands, IslandCrossing *ic,
  Island islandsInfo[40],
  int islandMap[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP], int type) {
  int targetIsland;

  //check to see if any island has the given requirement (type)
  for(targetIsland = 1; targetIsland < numIslands; targetIsland++) {
    bool caseSwitch = false;
    switch(type) {
      case GOLD:
        if(islandsInfo[targetIsland].gold) caseSwitch = true;
        break;

      case TOOL:
        if((islandsInfo[targetIsland].axe && !agent.axe) 
          || (islandsInfo[targetIsland].key && !agent.key)) {
          caseSwitch = true;
        }
        break;

      default:
        caseSwitch = true;
    }
    //if we found the wanted tool at the current island, check to see if we can path
    //by water to this island (using the shortest possible path)
    if(caseSwitch == true) {
      if(targetIsland != numIslands) {
        //first we check if we have enough stones on the current island to reach the new island
        if(islandsInfo[0].stones + agent.stones > 0) {
          ic->raftNeeded = false;
          ic->stonesNeeded = path_to_best_port(&(ic->islandPort.x),
            &(ic->islandPort.y), &(ic->homePort.x), &(ic->homePort.y),
            islandsInfo[targetIsland].firstSpaceSeen,
            islandsInfo[0].stones + agent.stones, 0, islandMap);
        }
        if(ic->homePort.x != -1) {
          ic->islandNum = targetIsland + 1;
          return 1;
        //if we can't reach by stones, try by raft (if we have one or there is a tree we can cut down)
        } else if((agent.raft ||
          (islandsInfo[0].tree && agent.axe)) &&
          islandsInfo[targetIsland].tree) {

          path_to_best_port(&(ic->islandPort.x), &(ic->islandPort.y),
            &(ic->homePort.x), &(ic->homePort.y),
            islandsInfo[targetIsland].firstSpaceSeen,
            islandsInfo[0].stones + agent.stones, 1, islandMap);

          if(ic->homePort.x != -1) {
            ic->raftNeeded = true;
            ic->stonesNeeded = 0;
            ic->islandNum = targetIsland + 1;
            return 1;
          }
        } 
      }
    }
  }
  return 0;
}

//Check if an island takes less stones to reach than it has on the island.
//if so we are gaining more stones to explore it than we lose to get there so
//it's free real estate
int stone_island_check(int numIslands, IslandCrossing *ic,
  Island islandsInfo[40],
  int islandMap[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP]) {

  int targetIsland;
  int bestStoneValue = -1;
  int stonesUsed = 0;
  int bestStonesUsed;
  int chosenIsland;
  int bestIslandPortX, bestIslandPortY, bestHomePortX, bestHomePortY;
  //check if an island has stones and we have stones to travel to it
  for(targetIsland = 1; targetIsland < numIslands; targetIsland++) {
    if(islandsInfo[targetIsland].stones > 0) {
      if(islandsInfo[0].stones + agent.stones > 0) {
        stonesUsed = path_to_best_port(&(ic->islandPort.x),
          &(ic->islandPort.y), &(ic->homePort.x), &(ic->homePort.y),
          islandsInfo[targetIsland].firstSpaceSeen,
          islandsInfo[0].stones + agent.stones, 0, islandMap);
      }
      //if the island is reachable and the shortest path we found takes less stones
      //to traverse than there are stones on the island
      if(ic->homePort.x != -1 &&
        islandsInfo[targetIsland].stones - stonesUsed >
        bestStoneValue) {

        bestHomePortX = ic->homePort.x;
        bestHomePortY = ic->homePort.y;
        bestIslandPortX = ic->islandPort.x;
        bestIslandPortY = ic->islandPort.y;
        bestStonesUsed = stonesUsed;
        bestStoneValue =
          islandsInfo[targetIsland].stones - stonesUsed;
        chosenIsland = targetIsland + 1;
      }  
    }
  }
  if(bestStoneValue >= 0) {
    ic->islandNum = chosenIsland;
    ic->homePort.x = bestHomePortX;
    ic->homePort.y = bestHomePortY;
    ic->islandPort.x = bestIslandPortX;
    ic->islandPort.y = bestIslandPortY;
    ic->stonesNeeded = bestStonesUsed;
    ic->raftNeeded = false;
    return 1;
  }
  return 0;
}

//For a chosen island attempt to path to the current island
int path_to_best_port(int *bestTargetPortX, int *bestTargetPortY,
  int *bestHomePortX,int *bestHomePortY, Coords startPos, int stones,
  bool raft, int islandMap[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP]) {

  Coords spaces[DOUBLE_MAX_MAP * DOUBLE_MAX_MAP];
  int seen[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP] = {{0}};
  spaces[0].x = startPos.x;
  spaces[0].y = startPos.y;
  seen[spaces[0].y][spaces[0].x] = 1;
  int bestPathLength = MAX_MAP * MAX_MAP;
  int i,j;
  int counter = 1;
  int homePortX = -1; 
  int homePortY = -1;
  int stonesUsed;
  int result = MAX_DIST;

  int waterDir = adjacent_to(startPos.y, startPos.x, '~');
  if(waterDir != -1) {
    result = plan_water_path(startPos.x, startPos.y, waterDir, &homePortX, &homePortY, 1, stones, raft, islandMap);
    if(result < bestPathLength && (result > 0 && is_reachable(agent.location, homePortX, homePortY)) || (result >= 0 && agent.currentlyRafting)) {
      bestPathLength = result;
      *bestTargetPortX = startPos.x;
      *bestTargetPortY = startPos.y;
      *bestHomePortX = homePortX;
      *bestHomePortY = homePortY;
    }  
  }
  for(i = 0; i < counter; i++) {
    for(j = 0; j < 4; j++) {
      int dir = j; 
      int forwardX, forwardY;
      result_of_move(spaces[i].x, spaces[i].y, dir,
        &forwardX, &forwardY);
      if(out_of_bounds(forwardX, forwardY, DOUBLE_MAX_MAP)) continue;
      waterDir = adjacent_to(forwardY, forwardX, '~');
      if(is_impassable_island_time(Wmap[forwardY][forwardX]) == false
        && waterDir != -1) {
       result = plan_water_path(forwardX, forwardY, waterDir, &homePortX, &homePortY, 1, stones, raft, islandMap);
        if(result < bestPathLength && (result > 0 && is_reachable(agent.location, homePortX, homePortY)) || (result >= 0 && agent.currentlyRafting)) {
          bestPathLength = result;
          *bestTargetPortX = forwardX;
          *bestTargetPortY = forwardY;
          *bestHomePortX = homePortX;
          *bestHomePortY = homePortY;
        }  
      }
      if(seen[forwardY][forwardX] == 1 ||
        is_impassable_island_time(Wmap[forwardY][forwardX])) continue;
      else {
        spaces[counter].x = forwardX;
        spaces[counter].y = forwardY;
        seen[forwardY][forwardX] = 1;
        counter++;
      }
    }
  }
  if(bestPathLength > 0) return bestPathLength;
  else return 0;
}

//set up for A* search
char search_setup(int targetX, int targetY, bool waterSearch) {
  int predecessor[DOUBLE_MAX_MAP * DOUBLE_MAX_MAP];
  int costToReach[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP] = {{0}};

  Stack s = newStack();
  pushOnto(s, agent.location.x, agent.location.y, agent.direction, 0,
    adjusted_manhattan_dist(targetX, targetY, agent.location.x,
    agent.location.y, agent.direction));
  costToReach[agent.location.y][agent.location.x] = -1;

  Astar_search(s, targetX, targetY, predecessor, costToReach,
    waterSearch);

  disposeStack(s);
  return map_path(predecessor, targetX, targetY);
}

//Uses A* search to a given square
void Astar_search(Stack s, int targetX, int targetY,
  int predecessor[DOUBLE_MAX_MAP * DOUBLE_MAX_MAP], 
  int costToReach[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP],
  bool waterSearch) {

  int xPos, yPos, dir, distFromStart, heuristicCost;
  popFrom(s, &xPos, &yPos, &dir, &distFromStart, &heuristicCost);
  //pop the space from the stack with the lowest heuristic cost (distFromStart + adjusted_manhattam_dist())
  costToReach[yPos][xPos] = distFromStart;
  int newXPos, newYPos, newDir, newDistFromStart, newHeuristicCost;
  int i;
  //look at the 4 spaces adjacent to this one (only looking behind the bot on the first space as
  //after that it will be the space we just came from)
  for(i = 0; i < NUM_DIRECTIONS; i++) {
    newDir = (dir + i) % 4; 
    result_of_move(xPos, yPos, newDir, &newXPos, &newYPos);
    switch(i) {
      case UP:
        newDistFromStart = distFromStart + 1;
        break;
      case RIGHT: case LEFT:
        newDistFromStart = distFromStart + 2;
        break;
      case DOWN:
        if(xPos == agent.location.x && yPos == agent.location.y) {
          newDistFromStart = distFromStart + 3;
        }
        else continue;
    }
    //check that this space is passable (or is water if we are water searching)
    bool impassable = true;
    if(waterSearch == true) {
      if(Wmap[newYPos][newXPos] == '~') impassable = false;
    } else if((targetY == AGENT_START && targetX == AGENT_START) || 
      (targetY == agent.seenGold.y && targetX == agent.seenGold.x)) {
      impassable = is_impassable_island_time(Wmap[newYPos][newXPos]);
    } else impassable = is_impassable(Wmap[newYPos][newXPos]);

    //if the current space is passable and this is the shortest path we have found to it so far
    //(tracked using CostToReach) then save its cost and the predecessor to it (so we can backtrack through the
    //route taken once we find the target)
    if(impassable != true ||
      (newXPos == targetX && newYPos == targetY)) {
      if(costToReach[newYPos][newXPos] == 0 ||
        costToReach[newYPos][newXPos] > newDistFromStart) {

        costToReach[newYPos][newXPos] = newDistFromStart;
        predecessor[newXPos + DOUBLE_MAX_MAP * newYPos] =
          xPos + DOUBLE_MAX_MAP * yPos;
        //If the space is the target then we can stop searching.
        if(newXPos == targetX && newYPos == targetY) {
          return;
        }
        //if not the target, add it to the stack so we can search from it in future iterations.
        newHeuristicCost = newDistFromStart +
        adjusted_manhattan_dist(targetX, targetY, newXPos, newYPos, newDir);
        pushOnto(s, newXPos, newYPos, newDir,
          newDistFromStart, newHeuristicCost);
      }
    }
  }
  return Astar_search(s, targetX, targetY, predecessor,
    costToReach, waterSearch);
}

//given a list of predecessors map the path to find the next action needed to travel that path.
//We do this by backtracking through the predecessor list.
char map_path(int predecessor[DOUBLE_MAX_MAP * DOUBLE_MAX_MAP],
  int currX, int currY) {
  int predX =
    predecessor[currX + DOUBLE_MAX_MAP * currY] % DOUBLE_MAX_MAP;
  int predY =
    predecessor[currX + DOUBLE_MAX_MAP * currY] / DOUBLE_MAX_MAP;

  if(predX == agent.location.x && predY == agent.location.y) {
    int i;
    for(i = 0; i < NUM_DIRECTIONS; i++) {
      int dir = (agent.direction + i) % 4;
      int resultX, resultY;
      result_of_move(predX, predY, dir, &resultX, &resultY);
      if(resultX == currX && resultY == currY) break;
    }

    switch(i) {
      case UP:
        return 'f';
        break;
      case RIGHT: case DOWN: 
        return 'r';
        break;
      case LEFT:
        return 'l';
    }
  }

  return map_path(predecessor, predX, predY);
}

//search for a certain island number using BFS.
//We cannot use A* as we do not know which individual square we are mapping to.
//But this still guarantees an optimal length route
//stack is prioritised based on distance from the start rather than a heuristic
bool BFS_water_search(Stack s,
  int costToReach[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP],
  int predecessor[DOUBLE_MAX_MAP * DOUBLE_MAX_MAP],
  int *foundPortX, int *foundPortY, int targetIsland,
  int islandMap[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP]) {

  int xPos, yPos, dir, distFromStart, heuristicCost;
  //if e run out of spaces to check then we have not found a path
  if(s->top == NULL) return 0;
  popFrom(s, &xPos, &yPos, &dir, &distFromStart, &heuristicCost);

  costToReach[yPos][xPos] = distFromStart;
  int newXPos, newYPos, newDir, newDistFromStart, newHeuristicCost;
  int i;
  //search radially outwards from the current position
  for(i = 0; i < NUM_DIRECTIONS; i++) {
    newDir = (dir + i) % 4; 
    result_of_move(xPos, yPos, newDir, &newXPos, &newYPos);
    switch(i) {
      case UP: case RIGHT: case LEFT:
        newDistFromStart = distFromStart + 1;
        break;
      case DOWN:
        if(xPos == agent.location.x && yPos == agent.location.y) {
          newDistFromStart = distFromStart + 1;
        }
        else continue;
    }
    //as we are only using water we only store water spaces as possible path nodes
    if(Wmap[newYPos][newXPos] == '~' ||
      islandMap[newYPos][newXPos] == targetIsland) {
      if(costToReach[newYPos][newXPos] == 0 ||
        costToReach[newYPos][newXPos] > newDistFromStart) {

        costToReach[newYPos][newXPos] = newDistFromStart;
        predecessor[newXPos + DOUBLE_MAX_MAP * newYPos] =
          xPos + DOUBLE_MAX_MAP * yPos;
      //if the given space is on the island we want to find, then we hve found an optimal path
        if(islandMap[newYPos][newXPos] == targetIsland)  {
          *foundPortX = newXPos;
          *foundPortY = newYPos;
          return 1;
        }
        //else, push this new space onto the stack
        newHeuristicCost = newDistFromStart;
        pushOnto(s, newXPos, newYPos, newDir,
          newDistFromStart, newHeuristicCost);
      }
    }

  }

  return BFS_water_search(s, costToReach, predecessor, foundPortX, 
    foundPortY, targetIsland, islandMap);
}

//setup for BFS_water_search()
//equivalent to search_setup() for the A* search
int plan_water_path(int islandPortX, int islandPortY, int dir,
  int *foundPortX, int *foundPortY, int targetIsland, 
  int stones, bool raft, int islandMap[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP]) {

  int predecessor[DOUBLE_MAX_MAP * DOUBLE_MAX_MAP];
  int costToReach[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP] = {{0}};
  Stack s = newStack();
  //push the island port onto the stack as our starting port for the search for the agent's current island
  pushOnto(s, islandPortX, islandPortY, dir, 0, 0);
  costToReach[islandPortY][islandPortX] = -1;
  int result = BFS_water_search(s, costToReach, predecessor, foundPortX,
    foundPortY, targetIsland, islandMap);
  disposeStack(s);
  if(result) {
    //if we found a route and to the island, make sure we have the resources to get there
    int stonesUsed = 0;
    count_stones(predecessor, *foundPortX, *foundPortY, islandPortX,
      islandPortY, &stonesUsed);
    if(!raft && (stones && stonesUsed > stones)) return -1;
    else return stonesUsed;
  }
  return -1;
}

//count the number of stones used to traverse a path
//by counting the number of water spaces traversed
void count_stones(int predecessor[DOUBLE_MAX_MAP * DOUBLE_MAX_MAP],
  int currX, int currY, int startingX, int startingY, int *stones) {

  int predX =
    predecessor[currX + DOUBLE_MAX_MAP * currY] % DOUBLE_MAX_MAP;
  int predY =
    predecessor[currX + DOUBLE_MAX_MAP * currY] / DOUBLE_MAX_MAP;

  if(predX == startingX && predY == startingY) return;
  (*stones)++;
  return count_stones(predecessor, predX, predY, startingX,
    startingY, stones);
}

//create new node
static Link newNode(int xPos, int yPos, int dir,
  int distFromStart, int heuristicCost) {

  Link new = malloc(sizeof(searchNode));
  assert(new != NULL);
  new->xPos = xPos;
  new->yPos = yPos;
  new->dir = dir; 
  new->distFromStart = distFromStart;
  new->heuristicCost = heuristicCost;
  new->next = NULL;
  return new;
}

//create stack
Stack newStack() {
  Stack new = malloc(sizeof(StackRep));
  assert(new != NULL);
  new->top = NULL;
  return new;
}

//free stack
void disposeStack(Stack s) {
  if (s == NULL) return;
  Link next, curr = s->top;
  while (curr != NULL) {
    next = curr->next;
    disposeNode(curr);  
    curr = next;
  }
}

//pop the item from the top of the priority stack
void popFrom(Stack s, int *xPos, int *yPos, int *dir,
  int *distFromStart, int *heuristicCost) {

  assert (s != NULL && s->top != NULL);
    *xPos = s->top->xPos;
    *yPos = s->top->yPos;
    *dir = s->top->dir;
    *distFromStart = s->top->distFromStart;
    *heuristicCost = s->top->heuristicCost;
  Link old = s->top;
  s->top = old->next;
  free(old);
  return;
}

//push a new node onto the priority stack. Prioritised in order of manhattan distance from the target
void pushOnto(Stack s, int xPos, int yPos, int dir,
  int distFromStart, int heuristicCost) {
  //create a new node
  Link new = newNode(xPos, yPos, dir, distFromStart, heuristicCost);
  Link curr = s->top;
  //place the node in its appropriate place in the queue (prioritised by heuristic cost)
  if(curr == NULL) {
    s->top = new;
    return;
  } else if(curr->next == NULL) {
    if(heuristicCost < s->top->heuristicCost) {
      new->next = s->top;
      s->top = new;
    } else s->top->next = new;
    return;
  }
  while(curr->next != NULL &&
    curr->next->heuristicCost < heuristicCost) {

    curr = curr->next;
  }
  new->next = curr->next;
  curr->next = new;

  return;
}
//free node
static void disposeNode(Link curr) {
  assert(curr != NULL);
  free(curr);
}

//check if a space is reachable from the agent's current location by land
int is_reachable(Coords agentLocation, int targetX, int targetY) {
  if(agent.location.x == targetX && agent.location.y == targetY) return 1;
  Coords spaces[DOUBLE_MAX_MAP * DOUBLE_MAX_MAP];
  int seen[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP] = {{0}};
  spaces[0].x = agentLocation.x;
  spaces[0].y = agentLocation.y;

  int counter = 1;
  int i, j;

  //check radially out from the current position
  for(i = 0; i < counter; i++) {
    for(j = 0; j < 4; j++) {
      int dir = (agent.direction + j) % 4; 
      int forwardX, forwardY;
      result_of_move(spaces[i].x, spaces[i].y, dir,
        &forwardX, &forwardY);

      if(seen[forwardY][forwardX] == 1) continue;
      //if we have pathed to the target using only passable spaces then the target is reachable
      if(forwardY == targetY && forwardX == targetX) {
        return 1;
      } 
      //if the space is passable and reachable by land, add it to queue search from
      if(is_impassable(Wmap[forwardY][forwardX]) == 0) {
        spaces[counter].x = forwardX;
        spaces[counter].y = forwardY;
        seen[forwardY][forwardX] = 1;
        counter++;
      }
    }
  }
  return 0;
}

//find the closest reachable space of a given type by land
bool closest_reachable(Coords agentLocation, int *x,
  int *y, int type) {

  Coords spaces[DOUBLE_MAX_MAP * DOUBLE_MAX_MAP];
  int seen[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP] = {{0}};
  spaces[0].x = agentLocation.x;
  spaces[0].y = agentLocation.y;
  seen[spaces[0].y][spaces[0].x] = 1;
  int closest_unseen = MAX_DIST;
  int dist = MAX_DIST;

  int counter = 1;
  int i, j;
  //search radially outwards from the current space
  for(i = 0; i < counter; i++) {
    for(j = 0; j < 4; j++) {
      int dir = (agent.direction + j) % 4; 
      int forwardX, forwardY;
      result_of_move(spaces[i].x, spaces[i].y, dir,
        &forwardX, &forwardY);
      if(seen[forwardY][forwardX] == 1) continue;

      bool case_switch = true;

      switch(type) {
        case UNSEEN:
          if(Wmap[forwardY][forwardX] != 0) case_switch = false;
          break;
        case UNSEEN_WATER:
          if(Wmap[forwardY][forwardX] != '~' ||
            placesBeen[forwardY][forwardX] != 0 || 
            !adjacent_to(forwardY, forwardX, 0)) {
              case_switch = false;
            }
          break;

        case PERIMETER:
          if(is_impassable(Wmap[forwardY][forwardX]) ||
            placesBeen[forwardY][forwardX] == 1) {
            case_switch = false;
          }
          break;
        default:
          if(Wmap[forwardY][forwardX] != type) case_switch = false;
      }
 
      if(case_switch == true) {
        if(type == PERIMETER &&
          adjacent_to_impassable(forwardY, forwardX) == -1) continue;
          //if the space found passes the requirements of its type and it is the closest one
          //that fulfil that types requirements then we save its coordinates to be returned
          //and later pathed to
        dist = adjusted_manhattan_dist(forwardX, forwardY,
          agentLocation.x, agentLocation.y, agent.direction);
        if(dist < closest_unseen) {
          closest_unseen = dist;
          *x = forwardX;
          *y = forwardY;
        } 
      } else {
        switch(type) {
          case UNSEEN_WATER:
            if(Wmap[forwardY][forwardX] == '~') case_switch = true;
            break;
          default:
            if(is_impassable(Wmap[forwardY][forwardX]) == 0) {
              case_switch = true;
            }
            break;
        }
        //otherwise as long as the space is passable (by land unless we are looking for water)
        //we add it to be searched from
        if(case_switch == true) {
          spaces[counter].x = forwardX;
          spaces[counter].y = forwardY;
          seen[forwardY][forwardX] = 1;
          counter++;
        }
      }

    }
      
    if(closest_unseen != MAX_DIST) break;
  }
  if(closest_unseen == MAX_DIST) return false;
  return true;
}


//manhattan distance calculation adjusted to account for the cost of turning
//e.g. if the space is directly behind the agent it takes 3 turns to get there not 1.
int adjusted_manhattan_dist(int x1, int y1, int x2, int y2, int dir) {
  int result = 0;
  if(dir >= 0 && dir <= 3) {
    if(y2 < y1 && dir == UP) result += 2;
    else if(y2 > y1 && dir == DOWN) result += 2;
    else if(x2 < x1 && dir == LEFT) result += 2;
    else if(x2 > x1 && dir == RIGHT) result += 2;
    else if(y1 != y2 && (dir == LEFT || dir == RIGHT)) result += 1;
    else if(x1 != x2 && (dir == UP || dir == DOWN)) result += 1; 
  }
  result += abs(x1 - x2) + abs(y1 - y2);
  return result;
}

//checks if there is the given char in a space adjacent to a given space
int adjacent_to(int yCoord, int xCoord, char target) { 
  int dir;
  for(dir = 0; dir < 4; dir++) {
    int forwardX, forwardY;
    result_of_move(xCoord, yCoord, dir, &forwardX, &forwardY);
    if(Wmap[forwardY][forwardX] == target) {
      return dir;
    }
  }
  return -1;
}

//checks if any of the squares adjacent to a given square are impassable
//used for determining a space on the perimeter of an island
int adjacent_to_impassable(int yCoord, int xCoord) {
  int dir;
  for(dir = 0; dir < 4; dir++) {
    int forwardX, forwardY;
    result_of_move(xCoord, yCoord, dir, &forwardX, &forwardY);
    if(Wmap[forwardY][forwardX] != 'o' &&
      Wmap[forwardY][forwardX] != 'T' && 
      is_impassable(Wmap[forwardY][forwardX])) return dir;
  }

  return -1;
}

//check if the current square is adjacent to the port
int adjacent_to_island_port(int yCoord, int xCoord,
  int portY, int portX) {
  int dir;
  for(dir = 0; dir < 4; dir++) {
    int forwardX, forwardY;
    result_of_move(xCoord, yCoord, dir, &forwardX, &forwardY);
    if(forwardY == portY && forwardX == portX) {
      return dir;
    }
  }
  return -1;
}

//checks if x and y coords are within  the possible range
int out_of_bounds(int x, int y, int max) {
  return x < 0 || y < 0 ||  x >= max || y >= max;
}

//check if the space is passable
//trees and stones are treated as impassable for normal traversal
int is_impassable(char space) {
  return space == '.' || space == '*' || space == '~' ||
  space == 'T' || space == 0 || space == 'o' ||
  (space == '-' && !agent.key);
}

//adapted version of is_impassable() to consider stones and trees (if we have an axe)
//as passable. We usually avoid pathing through stones and trees to make sure they are used
//optimally for water travel but in some cases that limits our options needlessly. 
int is_impassable_island_time(char space) {
  return space == '.' || space == '*' || space == '~' ||
  (space == 'T' && !agent.axe) || space == 0 ||
  (space == '-' && !agent.key);
}

//resets a 2d array so all elements are 0
void reset(int map[DOUBLE_MAX_MAP][DOUBLE_MAX_MAP]) {
  int i,j;
  for(i = 0; i < DOUBLE_MAX_MAP; i++) {
    for(j = 0; j < DOUBLE_MAX_MAP; j++) {
      map[i][j] = 0;
    }
  }
}

//check if two spaces are on the same island
bool on_same_island(int x1, int y1, int x2, int y2) {
  return islandMap[y1][x1] == islandMap[y2][x2];
}
