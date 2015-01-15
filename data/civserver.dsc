# GGZ Server game description file for Freeciv

[GameInfo]
Author = See http://www.freeciv.org/wiki/People
Description = Freeciv strategy game
Homepage = http://www.freeciv.org/
Name = Freeciv
Version = 2.5.0-beta2

[LaunchInfo]
ExecutablePath = ${exec_prefix}/bin/freeciv-server -q 180 -e

[Protocol]
Engine = Freeciv
Version = +Freeciv-2.5-network

[TableOptions]
AllowLeave = 1
# Freeciv bots are handled internally, but aren't visible to GGZ
#BotsAllowed = 
PlayersAllowed = 1..30
# Because of -q 180 -e options, above
KillWhenEmpty = 0
AllowSpectators = 0
AllowRestore = 1

[Statistics]
Records = 1
Ratings = 1