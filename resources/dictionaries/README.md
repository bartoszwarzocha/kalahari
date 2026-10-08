# Spelling dictionaries

Hunspell dictionaries shipped with Kalahari. `SpellCheckService` finds them in the
`dictionaries` folder of the application's resources.

| Files | Language | Source | License |
|---|---|---|---|
| `pl_PL.aff`, `pl_PL.dic` | Polish | LibreOffice dictionaries 24.2.1 (Ubuntu package `hunspell-pl` 1:24.2.1-1), word list of the sjp.pl project, copyright Marek Futrega | Apache License 2.0 – see `LICENSE_pl_PL.txt` |
| `en_US.aff`, `en_US.dic` | English (US) | SCOWL by Kevin Atkinson (Ubuntu package `hunspell-en-us` 1:2020.12.07-2) | SCOWL license (permissive, BSD-like) – see `LICENSE_en_US.txt` |

## Polish dictionary: license choice

The Polish dictionary is offered under a choice of licenses: GPL, LGPL, MPL, Apache 2.0 or
Creative Commons ShareAlike. Kalahari uses it under the **Apache License 2.0**, which is
permissive and compatible with Kalahari's MIT license.

## Changes

`pl_PL.aff` and `pl_PL.dic` were converted from ISO 8859-2 to UTF-8 (`SET UTF-8` in
`pl_PL.aff`), so that the dictionary works with the UTF-8 text the editor passes to
Hunspell. The words and rules are unchanged. `en_US` is unchanged.
