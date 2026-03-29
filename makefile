# ==========================================
# Limbus Company News Archive - Pipeline
# ==========================================

INPUT_MASTER = lcsn-MASTER.txt
STEAM_LIST = full_steamlist.txt
SORTED_TXT = lcsn-sorted_notices.txt
NOTICES_JSON = notices.json

.PHONY: all

all:
	@chmod +x lc-steam-news-sorter z_split_notices_for_google_docs
	@echo "\n--- [1/3] Sorting Notices ---"
	./lc-steam-news-sorter $(INPUT_MASTER) $(STEAM_LIST)
	
	@echo "\n--- [2/3] Chunking for Google Docs ---"
	./z_split_notices_for_google_docs $(SORTED_TXT)
	
	@echo "\n--- [3/3] Generating JSON & Metadata ---"
	python3 txt_to_json.py $(SORTED_TXT) $(NOTICES_JSON) $(STEAM_LIST)