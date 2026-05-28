# Echo `-e` backslash escape note (BNFC vs tokenizer)

## “What fails” (the exact test)
The BusyBox shell test suite expects this to work:

```sh
./busybox_shell echo -e 'a\\nb' | grep -q '^b$'
```

**Meaning**
- `echo -e` must interpret `\\n` in the *argument token* as a real newline.
- Therefore the output must be two lines: `a` then `b`.
- `grep '^b$'` succeeds only if there is a line that is exactly `b`.

---

## Blocker: what changed after moving to BNFC
Earlier, when command lines were parsed using a tokenizer/splitter approach, the argument token bytes reaching `cmd_echo.c` matched the expectations of the original `echo -e` implementation.

After switching the shell frontend to **BNFC**, the shell still builds argv from the parsed AST, but the **backslash representation inside argv tokens is not guaranteed to be identical** at the byte level.

Consequence:
- the old `echo -e` logic was **too strict**
- it recognized only a narrow encoding form of backslash escapes
- if BNFC delivered the same intended text with an equivalent but differently-encoded backslash representation, then `\\n` would not be converted to an actual newline
- output didn’t split into the expected lines, so `grep '^b$'` failed

---

## Fix: what we changed
### File
`busybox_shell/cmd_echo.c`

### Change summary
- Refactored escape interpretation into a helper.
- Extended `echo -e` escape handling so it recognizes common alternative argv encodings that BNFC may deliver.
- Applied the same interpretation consistently for normal output and the `--json` output path.

---

## “Pics” / intuition (ASCII)

### Before (tokenizer argv preservation)
```
input string
  -> tokenizer splitter
  -> argv token bytes match echo's old assumptions
  -> echo -e converts \n -> newline
```

### After (BNFC grammar -> AST -> argv)
```
input string
  -> BNFC parser (grammar)
  -> AST
  -> adapter builds argv from AST
  -> backslashes may arrive in a different (but equivalent) argv encoding
  -> echo -e is updated to be resilient to these forms
```

---

## Why this stays grammar-based (BNFC still parses)
This fix does **not** revert command parsing back to tokenizers.

- BNFC still produces the AST.
- The adapter still converts AST nodes into `argc/argv`.
- Only the `echo` builtin’s **post-parsing escape decoding** was made tolerant to BNFC-delivered argv/backslash representation.

