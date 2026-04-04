import re
from collections import Counter

file_path = "lcsn-MASTER.txt"
word_counter = Counter()

with open(file_path, "r", encoding="utf-8") as f:
    text = f.read()

notices = re.findall(r"Writing Sample \d+:\s*‘(.*?)’", text, re.DOTALL)

for content in notices:
    words = re.findall(r'\b\w+\b', content.lower())
    word_counter.update(words)

print("Top words:")
for word, count in word_counter.most_common(50):
    print(f"{word}: {count}")

print(f"\nTotal unique words: {len(word_counter)}")