#!/usr/bin/env python3
"""Remove 'return <expr>' from void @test functions.

Simple approach: find lines matching '    return <expr>' inside @test functions
and either remove them (for constants) or convert to assertEquals.
"""

import os
import re

FIXTURES_DIR = "tests/fixtures"

def process_file(filepath):
    with open(filepath) as f:
        content = f.read()
    
    lines = content.split('\n')
    new_lines = []
    in_test_func = False
    
    for i, line in enumerate(lines):
        stripped = line.strip()
        
        # Detect @test annotation
        if stripped == '@test':
            in_test_func = True
            new_lines.append(line)
            continue
        
        # Detect function declaration after @test
        if in_test_func and re.match(r'^func\s+\w+\s*\(.*?\)\s*:\s*void', stripped):
            in_test_func = True  # still in test func
            new_lines.append(line)
            continue
        
        if in_test_func:
            # Check if this is a return statement at the top level
            # (indented by 4 spaces, not inside nested blocks)
            m = re.match(r'^    return\s+(.*?)\s*$', stripped)
            if m:
                expr = m.group(1)
                # Simple constant - remove
                if re.match(r'^\d+$', expr) or re.match(r'^0x[0-9a-fA-F]+$', expr):
                    continue
                # Has function calls or assignments - keep as statement
                if '(' in expr or '=' in expr:
                    new_lines.append(f'    {expr}')
                    continue
                # Variable or expression - convert to assertEquals
                new_lines.append(f'    assertEquals(0, {expr}, "{os.path.basename(filepath)}")')
                continue
            
            # Exit test function when we see closing brace at top level
            if stripped == '}':
                in_test_func = False
        
        new_lines.append(line)
    
    with open(filepath, 'w') as f:
        f.write('\n'.join(new_lines))

def main():
    fixtures = sorted(os.listdir(FIXTURES_DIR))
    fixtures = [f for f in fixtures if f.endswith('.ae')]
    
    for fixture in fixtures:
        filepath = os.path.join(FIXTURES_DIR, fixture)
        process_file(filepath)
        print(f"  Processed: {fixture}")

if __name__ == '__main__':
    main()
