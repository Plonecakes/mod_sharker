++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++@@@@@+@@@+@@++++++++++++++++@@@+@+@++@++@=.+@+@+@@@+@=.+++++@=.+@+++@@@+@.+@+@@@++
++@+@+@+@+@+@+@+ mod_sharker +`-.+@@@+@-@+@=`+@@++@=++@=`+@@@+@=`+@+++@+@+@+\@+@=+++
++@+++@+@@@+@@++++++++++++++++@@@+@+@+@+@+@+@+@+@+@@@+@+@+++++@+++@@@+@@@+@++@+@@@++
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
This is mod_sharker by Plonecakes.
A homage to Game Shark, I think.

This is a PUBLIC, OPEN SOURCE mod. Feel free to redistribute. ^^
The source and latest version are available here:
                    https://github.com/Plonecakes/mod_sharker/

Unpack this to your Mabinogi folder (if you want) and inject this with Abyss's
LoadDLL functionality or whatever you use.

This mod is meant to make patching easier, after one does the research. They may
also easily share their patch with others.

Included is an example configuration, that is hopefully straight-forward.

In the main ini, mod_sharker.ini, there is an [Options] section which, underneath,
stores the options for the mod. Every other header is considered a patch. You may
specifify an [Options] header for other INIs as well, but only with respect to the
mods that INI includes.

In the options, you may enable or disable a mod, by name, similar to any memory
patch. If there is no entry for the patch, it is assumed to be on.

You may also use the LoadINI or LoadFolder constructions. Both are exampled. To
list multiples, separate the entries with commas and no spaces.

In patches, you may list multiple search and replace pairs by a specific index.
This index starts at 1 and counts upwards. The format within the entries is
simple hex. Spacing is optional but suggested. To use a wildcard, enter ?? instead
of hex. Also, you may select which memory section to search with the form Section#.
It will default to the code section. The options are code, data, a section by
internal name, or a section by index.

You may dynamically alter the search or replacement by the use of variables. They
are placed in the hex string in one of the forms:
  <Name:Size or Default>
  <Name:Size map {...} or Default>
  <Name:Size ?= Offset or Default>
If Name is omitted, the current section name is used, and the colon must still
exist. Size is always required, it is the amount of bytes to insert. If you omit
the "or" and a default value, the entry in options will be required of the user.
In the map form, you may enter a list of value mappings. The values here are in
decimal. However you may use a hexadecimal byte sequence with a preceding x.
The format is one of:
  {1: 2, 3: 4}
  {1 = 2, 3 = 4}
  {1: x02 00 00 00, 3: x04 00 00 00}
Spaces are optional, of course. The left value is the value the user enters, and
the right value is the value searched for or written. Offset is a value to offset
by, and the ? in the format must be a method of adjustment, + or - or *. Again,
this value is in decimal. An example might be:
  <UserInput:4 map {1:2,2:34} or 2>
Where the entry in UserInput will be referenced for the final value that this
segment is replaced with. If the user enteres UserInput=1 then the value will be
2, and if it is UserInput=2 then the value with be 34. If the user enters anything
else, an error will be thrown. If UserInput does not exist, 2 will be assumed. Of
course, if it is UserInput=0, then the mod will not be enabled.

You may conditionalize the patching of a search and replace by adding a Condition#
entry to the heading. This can contain a check in the form:
  Variable ?= Value
Optionally conjoined by logical operations such as &&, ||, or ^^. The valid
comparison operators are: ==, !=, <=, >=, <, and >. Delimiting spaces are required.
Logical operations are processed linearly.

You may insert the relative address of a symbol with a $ or the absolute address
with a $$. Currently you may refer to LogMessage or HeaderName.#, in which the
latter the patch must have already been applied. This functionality will be more
useful later.

The log is created as mod_sharker.log in the Mabinogi folder.
