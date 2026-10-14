# Medical Information System (C++17)

Console app that stores patient demographics, clinical records, lab results and billing,
protected by authentication, role-based access control and an encrypted database file.

## Build & run
- VS Code: open this folder, press **Ctrl+Shift+B**, then run `./mis` in the terminal
  (Windows: `mis.exe`). F5 also works with the C/C++ extension (MinGW/gdb: change
  `externalConsole` to true if input doesn't work).
- Terminal: `g++ -std=c++17 src/main.cpp -o mis && ./mis`
- CMake: `cmake -B build && cmake --build build`

First run: enter a database passphrase, then create the first Admin account.
Data is saved to `medical.db` (encrypted) after every action.

## Structure
- `src/Models.h` – Person -> Patient, ClinicalRecord, LabResult, Invoice, Payment
- `src/Security.h` – salted password hashing, encryption, hex helpers
- `src/MedicalSystem.h` – users, roles, per-patient access lists, billing, audit log, save/load
- `src/main.cpp` – console menus

## Roles
Admin (everything) | Doctor (clinical + labs) | Nurse (register, labs) | Billing (invoices/payments, no clinical data).
Non-admins only see patients they were granted access to (creators are auto-granted).

## Security note
Crypto here is educational, written without external libraries. Before real use, swap in
libsodium/OpenSSL (Argon2id/bcrypt for passwords, AES-256-GCM for data) and
never handle real patient data without HIPAA/GDPR review.
