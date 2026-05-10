# License Notes

## Fab Standard License

Treat Fab Standard License packages as usable outside Unreal Engine unless the
asset/package is explicitly marked UE-Only or has separate restrictions.

Recommended manifest values:

```json
{
  "license": {
    "name": "Fab Standard License",
    "ueOnly": false,
    "redistribution": "embedded-only",
    "attributionRequired": false
  }
}
```

Do not redistribute source assets as a standalone asset library. Keep them
embedded in a project/game build or in the user's private resource vault.

## UE-Only

If a package is UE-Only, do not convert/use it in this engine. Put it in
`quarantine/` or leave it in `inbox/` until the user provides compatible rights.

## Unknown

If source, author, URL, or license is unknown, ask the user and keep the package
out of converted/project assets until resolved.
