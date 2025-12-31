# Text encoding rules

- All text files in this repository must be UTF-8.
- In Visual Studio, save files as UTF-8 (BOM optional).
- Git normalizes line endings to LF via `.gitattributes`.

## Local check

Run the UTF-8 validation script before committing:

```bash
python3 scripts/check_utf8.py
```
