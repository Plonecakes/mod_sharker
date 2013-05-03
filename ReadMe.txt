++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++@@@@@+@@@+@@++++++++++++++++@@@+@+@++@++@=.+@+@+@@@+@=.+++++@=.+@+++@@@+@.+@+@@@++
++@+@+@+@+@+@+@+ mod_sharker +`-.+@@@+@-@+@=`+@@++@=++@=`+@@@+@=`+@+++@+@+@+\@+@=+++
++@+++@+@@@+@@++++++++++++++++@@@+@+@+@+@+@+@+@+@+@@@+@+@+++++@+++@@@+@@@+@++@+@@@++
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
This is mod_sharker by Plonecakes.
A homage to Game Shark, I think.

This is a PUBLIC, OPEN SOURCE mod. Feel free to redistribute. ^^
The source and latest version are available here: https://github.com/Plonecakes/mod_sharker/

Unpack this to your Mabinogi folder (if you want) and inject this with Abyss's
LoadDLL functionality or whatever you use.

This mod is meant to make patching easier, after one does the research. They may
also easily share their patch with others.

Included is an example configuration, that is hopefully straight-forward.

In the main ini, mod_sharker.ini, there is an [Options] section which, underneath,
stores the options for the mod. Every other header is considered a patch.

In the options, you may enable or disable a mod, by name, similar to any memory
patch. If there is no entry for the patch, it is assumed to be on.

You may also use the LoadINI or LoadFolder constructions. Both are exampled. To
list multiples, separate the entries with commas and no spaces.

In patches, you may list multiple search and replace pairs by a specific index.
This index starts at 1 and counts upwards. The format within the entries is
simple hex. Spacing is optional but suggested. To use a wildcard, enter ?? instead
of hex.

The log is created as mod_sharker.log in the Mabinogi folder.
