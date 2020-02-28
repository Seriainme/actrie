# Project **actrie**

## What is actrie?

In the beginning, **actrie** is an implementation of Aho-Corasick automation, optimize for large scale multi-pattern.

Now, we support more types of pattern: anti-ambiguity pattern, anti-antonym pattern, distance pattern, alternation pattern, etc. You can combine all of them together.

## Pattern syntax

### 1. **plain** pattern:

> **abc**

### 2. **anti-ambiguity** pattern:

> center **(?&!** ambiguity **)**

### 3. **anti-antonym** pattern:

> **(?<!** antonym **)** center

### 4. **distance** pattern:

> prefix **.{min,max}** suffix

### 5. **alternation** pattern:

> pattern0 **|** pattern1

### 6. **wrapper** pattern:

> **(** pattern **)**
