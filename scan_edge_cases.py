import re
import difflib
import os

def clean_title(title):
    return title.strip(" \t\n\r‘'\"’")

def run_scanner():
    print("🔍 Scanning for anomalous and irregular notices...\n")
    
    overrides = set()
    if os.path.exists("manual_overrides.csv"):
        with open("manual_overrides.csv", "r", encoding="utf-8") as f:
            for line in f:
                if "^¬^" in line:
                    overrides.add(clean_title(line.split("^¬^")[0]))

    steam_titles = []
    with open("full_steamlist.txt", "r", encoding="utf-8") as f:
        steam_data = f.read().split("Share")
        for block in steam_data:
            lines = [x.strip() for x in block.strip().split('\n') if x.strip()]
            if not lines: continue
            
            title = None
            for i, line in enumerate(lines):
                if line == "Posted" and i + 2 < len(lines):
                    title_idx = i + 2
                    if lines[title_idx] == "Ran":
                        title_idx += 2
                        
                    if title_idx < len(lines):
                        title = lines[title_idx]
                    break
            
            if not title and len(lines) >= 3:
                title = max(lines[:4], key=len)
                
            if title:
                steam_titles.append(title)

    with open("lcsn-MASTER.txt", "r", encoding="utf-8") as f:
        master_data = f.read()

    pattern = re.compile(r"Writing Sample\s+(\d+):\s*\n([\s\S]*?)(?=\nWriting Sample\s+\d+:\s*\n|\Z)", re.MULTILINE)
    
    suspects = []

    for match in pattern.finditer(master_data):
        ws_id = match.group(1)
        body = match.group(2).strip()
        lines = [x.strip() for x in body.split('\n') if x.strip()]
        if not lines: continue
        
        m_title = clean_title(lines[0])
        
        if m_title in overrides:
            continue
            
        is_substring_match = False
        for s_title in steam_titles:
            if len(s_title) > 8 and len(m_title) > 8:
                if s_title.lower() in m_title.lower() or m_title.lower() in s_title.lower():
                    is_substring_match = True
                    break
                    
        if is_substring_match:
            continue
            
        matches = difflib.get_close_matches(m_title, steam_titles, n=3, cutoff=0.0)
        best_score = 0
        if matches:
            best_score = difflib.SequenceMatcher(None, m_title.lower(), matches[0].lower()).ratio()
            
        if best_score < 0.85:
            suspects.append((ws_id, m_title, best_score, matches))

    with open("edge_case_report.txt", "w", encoding="utf-8") as out:
        out.write("--- EDGE CASE REPORT ---\n")
        out.write("These notices have low match confidence with the Steam list and may require a manual override.\n\n")
        
        for ws_id, m_title, score, matches in suspects:
            msg = f"Writing Sample {ws_id} (Confidence: {score:.1%})\n"
            msg += f"Notice Title: {m_title}\n"
            msg += "Suggested Overrides (Copy one into manual_overrides.csv):\n"
            for m in matches:
                msg += f"{m_title}^¬^{m}\n"
            msg += "-"*70 + "\n"
            
            print(msg, end="")
            out.write(msg)
            
    if len(suspects) == 0:
        print("0 suspect notices found. Everything matches cleanly.")
    else:
        print(f"\nFound {len(suspects)} suspect notices. See 'edge_case_report.txt' to easily copy-paste fixes.")

if __name__ == "__main__":
    run_scanner()