# limbus-notice-pipeline

A C/Python toolchain for processing and structuring official Limbus Company notices into a searchable dataset.

This pipeline powers: https://lcna.whosmalikx.com


## Status

This project is actively used and maintained as part of a live archive workflow.

The pipeline is stable for current usage but continues to evolve as new notices and improvements are added.


## Overview

The pipeline processes a master archive of Limbus Company notices (transcribed via AI OCR) and converts it into structured data for web use.

Core functionality includes:
- chronological reconstruction using Steam metadata
- text normalization and cleanup
- dataset restructuring and relabeling
- JSON generation for dynamic site rendering
- backup chunk generation for external storage


## Pipeline

```
lcsn-MASTER.txt
    ↓
lc-steam-news-sorter.c
    ↓
lcsn-sorted_notices.txt
    ├── z_split_notices_for_google_docs.c
    └── txt_to_json.py → notices.json
```

Optional:
```
reverse_notices_oldest-to-newest.c
```


## Components

### lc-steam-news-sorter.c
Primary sorting engine.

- matches notices against Steam posts
- assigns dates using metadata or fallback parsing
- removes duplicates and normalizes formatting
- reconstructs chronological order

Outputs:
- `lcsn-sorted_notices.txt`
- `lcsn-sort_log.txt`


### txt_to_json.py
Converts sorted notices into structured JSON.

- extracts title, date, content
- performs Steam-based date matching
- assigns lightweight tags

Output:
- `notices.json`


### z_split_notices_for_google_docs.c
Splits the archive into smaller `.txt` chunks for backup and external storage.


### reverse_notices_oldest-to-newest.c
Utility for reversing and relabeling datasets.


## Usage

### Run full pipeline

```
make
```

### Manual

```
./lc-steam-news-sorter lcsn-MASTER.txt full_steamlist.txt
./z_split_notices_for_google_docs lcsn-sorted_notices.txt
python3 txt_to_json.py lcsn-sorted_notices.txt notices.json full_steamlist.txt
```


## Data

Input:
- `lcsn-MASTER.txt` — master archive (AI OCR transcription)
- `full_steamlist.txt` — Steam post reference data

Output:
- `lcsn-sorted_notices.txt`
- `notices.json`


## Notes

- Archive order is reconstructed, not assumed
- Steam titles and notice titles may differ
- Date extraction uses both metadata and fallback parsing
- OCR quality impacts matching accuracy


## License

This project is licensed under the GNU General Public License v3.0.  
See the LICENSE file for details.
