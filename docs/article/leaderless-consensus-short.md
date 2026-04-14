Leaderless Consensus Family Verification

I recently finished a focused verification pass on a small family of leaderless consensus algorithms in executable TLA++.

The work covers five variants, from an intentionally naive failure case for learning purposes to a prefix-commit design with ordered state propagation.

Sore is there to fail and make the weakness of a naive protocol explicit. Calm, Flat, and Most hold in the current finite models. Rush is the most advanced checked variant, using timeout-free prefix-based live consensus, and its executable liveness check now also covers the 3-node case with one majority-preserving disconnect while requiring every surviving node to commit a non-empty prefix.

What mattered most in practice was not just the algorithm ladder itself, but the fact that it was checked as one coherent family with explicit safety and liveness properties.

One practical takeaway stood out: shared data-structure work mattered much more than clever protocol-specific micro-optimizations. Flat/inplace containers improved performance in a meaningful way while keeping the algorithms readable.

By contrast, a more invasive inline-storage optimization for Value was much less compelling and not worth the complexity.

Full detailed article with algorithm illustrations: https://gridem.blogspot.com/2026/04/leaderless-consensus-family-verification.html

Code and notes: https://github.com/gridem/tlapp https://github.com/gridem/tlapp/tree/main/docs/leaderless-consensus
