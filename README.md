Treasure-Hunt-Agent
-

The obstacles and tools within the environment are represented as follows:

Obstacles  Tools
T 	tree    a 	axe
-	door	k	key
~	water	o	stepping stone
*	wall	$	treasure

The goal for the agent is to collect the treasure and return to it's starting square. The agent can only see in a 5x5 grid around it's current position and thus must explore to understand the map it is in.

The agent will be represented by one of the characters ^, v, <  or  >, depending on which direction it is pointing. The agent is capable of the following instructions:

L   turn left
R   turn right
F   (try to) move forward
C   (try to) chop down a tree, using an axe
U   (try to) unlock a door, using an key

When it executes an L or R instruction, the agent remains in the same location and only its direction changes. When it executes an F instruction, the agent attempts to move a single step in whichever direction it is pointing. The F instruction will fail (have no effect) if there is a wall or tree directly in front of the agent.

When the agent moves to a location occupied by a tool, it automatically picks up the tool. The agent may use a C or U instruction to remove an obstacle immediately in front of it, if it is carrying the appropriate tool. A tree may be removed with a C (chop) instruction, if an axe is held. A door may be removed with a U (unlock) instruction, if a key is held.

If the agent is not holding a raft or a stepping stone and moves forward into the water, it will drown.

If the agent is holding a stepping stone and moves forward into the water, the stone will automatically be placed in the water and the agent can step onto it safely. When the agent steps away, the stone will appear as an upper-case O. The agent can step repeatedly on that stone, but the stone will stay where it is and can never be picked up again.

Whenever a tree is chopped, the tree automatically becomes a raft which the agent can use as a tool to move across the water. If the agent is not holding any stepping stones but is holding a raft when it steps forward into the water, the raft will automatically be deployed and the agent can move around on the water, using the raft. When the agent steps back onto the land (or a stepping stone), the raft it was using will sink and cannot be used again. The agent will need to chop down another tree in order to get a new raft.

If the agent attempts to move off the edge of the environment, it dies.

To win the game, the agent must pick up the treasure and then return to its initial location.

To Run
--

In one terminal session:
java Step -p 31415 -i s0.in 

In the other:
java Agent -p 31415
