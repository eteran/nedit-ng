
#ifndef REPLACE_ALL_DEFAULT_SCOPE_H_
#define REPLACE_ALL_DEFAULT_SCOPE_H_

/* Default scope if selection exists when replace dialog pops up.
   "Smart" means "In Selection" if the selection spans more than
   one line; "In Window" otherwise. */
enum ReplaceAllDefaultScope {
    REPL_DEF_SCOPE_WINDOW,
    REPL_DEF_SCOPE_SELECTION,
    REPL_DEF_SCOPE_SMART
};

#endif
