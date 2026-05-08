# Tuff Chess

Tuff Chess is a hobby chess engine that supports UCI (Universal Chess Interface)
I would like my code to be beginner friendly (I am a bit of a beginner myself), so I'm working on adding more comments to document the code. If you are reading the code, start with `types.hpp` in the `src` folder

Features:
- Very fast pseudo-legal move generation (via bitboards) with legality filtering during search
- Negamax alpha beta search
- Basic Evaluation

Version 1.1.0
- Repetition Detection 
- Transposition Table (+154.4)
- Estimated Elo (Blitz): 1800-2200

Version 1.0.0:
- Pseudo Legal Move Generation
- Alpha Beta Negamax search
- Basic Evaluation Function
- Quiescence Search (+65.9) 
- Move ordering (+470.4) 