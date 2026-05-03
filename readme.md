# Tuff Chess

Tuff Chess is a hobby chess engine that supports UCI (Universal Chess Interface)

Features:
- Very fast pseudo-legal move generation (via bitboards) with legality filtering during search
- Negamax alpha beta search
- Basic Evaluation

Updates & Elo increase:
- Quiescence Search (+65.9) Note: Results suggests that QSearch may be stronger, as 3-fold repetition detection hasn't been implemented yet, making winning positions drawn due to ignorance on the engine's part.
- Move ordering (+470.4) 