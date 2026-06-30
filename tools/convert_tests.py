#!/usr/bin/env python3
"""Convert test fixtures: @test(expect=N) → @test, void return, add imports.

Uses regex on the full content string. No line-by-line parsing.
"""

import os
import re

FIXTURES_DIR = "tests/fixtures"

def process_file(filepath):
    with open(filepath) as f:
        content = f.read()
    
    name = os.path.basename(filepath)
    
    # 1. Replace @test(expect=N) with @test
    content = re.sub(r'@test\(expect=\d+\)', '@test', content)
    
    # 2. Add import "std/test" if not present
    has_import_test = 'import "std/test"' in content or "import 'std/test'" in content
    has_any_import = re.search(r'^import\s+', content, re.MULTILINE)
    
    if not has_import_test:
        if has_any_import:
            content = re.sub(r'^(import[^\n]*\n)(?!import)', r'\1import "std/test"\n', content, count=1, flags=re.MULTILINE)
        else:
            content = re.sub(r'^(//.*\n)*', r'\1import "std/test"\n\n', content, count=1)
    
    # 3. Change func(): u64 to func(): void for @test functions
    # Multi-line pattern: @test\nfunc name(params): u64 { → @test\nfunc name(params): void {
    content = re.sub(
        r'(@test\nfunc\s+\w+\s*\([^)]*\))\s*:\s*u64\s*\{',
        r'\1: void {',
        content
    )
    
    # 4. Remove '    return <constant>' lines
    content = re.sub(r'^    return \d+\s*$', '', content, flags=re.MULTILINE)
    content = re.sub(r'^    return 0x[0-9a-fA-F]+\s*$', '', content, flags=re.MULTILINE)
    
    # 5. Convert '    return <variable>' to 'assertEquals(0, <variable>, "name")'
    content = re.sub(
        r'^    return ([a-zA-Z_]\w*)\s*$',
        r'    assertEquals(0, \1, "' + name + r'")',
        content,
        flags=re.MULTILINE
    )
    
    # 6. Convert remaining '    return <expr>' to just '<expr>'
    content = re.sub(
        r'^    return (.+)$',
        r'    \1',
        content,
        flags=re.MULTILINE
    )
    
    with open(filepath, 'w') as f:
        f.write(content)

def main():
    fixtures = sorted(os.listdir(FIXTURES_DIR))
    fixtures = [f for f in fixtures if f.endswith('.ae')]
    
    for fixture in fixtures:
        filepath = os.path.join(FIXTURES_DIR, fixture)
        process_file(filepath)
        print(f"  Processed: {fixture}")

if __name__ == '__main__':
    main()
