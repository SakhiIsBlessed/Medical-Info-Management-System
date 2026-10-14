#include <iostream>
#include <string>
#include <sstream>
#include "MedicalSystem.h"

static const std::string DB_FILE = "medical.db";

// ---------- input helpers ----------
static std::string ask(const std::string& label) {
    std::cout << label << ": ";
    std::string s;
    if (!std::getline(std::cin, s)) { std::cout << "\n(input closed)\n"; std::exit(0); }
    return s;
}

static int askInt(const std::string& label) {
    try { return std::stoi(ask(label)); } catch (...) { return -1; }
}

static long long askCents(const std::string& label) {
    try { return static_cast<long long>(std::llround(std::stod(ask(label)) * 100.0)); }
    catch (...) { return -1; }
}

static std::vector<std::string> splitComma(const std::string& s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        size_t a = item.find_first_not_of(" \t"), b = item.find_last_not_of(" \t");
        if (a != std::string::npos) out.push_back(item.substr(a, b - a + 1));
    }
    return out;
}

// ---------- display ----------
static void printPatient(const Patient& p, bool clinical) {
    std::cout << "\n================ PATIENT #" << p.id() << " ================\n"
              << "Name:    " << p.name() << "\nDOB:     " << p.dob() << "   Gender: " << p.gender()
              << "\nPhone:   " << p.phone() << "\nAddress: " << p.address()
              << "\nBlood:   " << p.bloodType() << "\nAllergies: ";
    if (p.allergies().empty()) std::cout << "none recorded";
    for (size_t i = 0; i < p.allergies().size(); ++i) std::cout << (i ? ", " : "") << p.allergies()[i];
    std::cout << "\n";

    if (clinical) {
        std::cout << "\n--- Clinical records ---\n";
        if (p.records().empty()) std::cout << "(none)\n";
        for (const auto& r : p.records())
            std::cout << r.date << " | Dx: " << r.diagnosis << " | Tx: " << r.treatment
                      << " | Notes: " << r.notes << " | By: " << r.doctor << "\n";
        std::cout << "\n--- Lab results ---\n";
        if (p.labs().empty()) std::cout << "(none)\n";
        for (const auto& l : p.labs())
            std::cout << l.date << " | " << l.test << ": " << l.value << " " << l.unit << "\n";
    }

    std::cout << "\n--- Billing ---\n";
    if (p.invoices().empty()) std::cout << "(none)\n";
    for (const auto& i : p.invoices()) {
        std::cout << "Invoice #" << i.id << " - " << i.description << " | Total " << money(i.amountCents)
                  << " | Paid " << money(i.paidCents()) << " | Balance " << money(i.balanceCents()) << "\n";
        for (const auto& y : i.payments)
            std::cout << "    payment #" << y.id << "  " << y.date << "  " << money(y.amountCents)
                      << "  " << y.method << "  " << y.maskedRef << "\n";
    }
    std::cout << "=====================================\n";
}

static void report(bool ok, MedicalSystem& sys, const std::string& success) {
    std::cout << (ok ? success : "Error: " + sys.lastError()) << "\n";
}

// ---------- actions ----------
static void registerPatient(MedicalSystem& sys) {
    std::string name = ask("Full name"), dob = ask("Date of birth (YYYY-MM-DD)"),
                gender = ask("Gender"), phone = ask("Phone"), addr = ask("Address"),
                blood = ask("Blood type"), alg = ask("Allergies (comma separated, blank = none)");
    int id = sys.registerPatient(name, dob, gender, phone, addr, blood, splitComma(alg));
    if (id) std::cout << "Patient registered with ID " << id << ".\n";
    else std::cout << "Error: " << sys.lastError() << "\n";
}

static void viewPatient(MedicalSystem& sys) {
    int id = askInt("Patient ID");
    Patient* p = sys.getPatient(id);
    if (!p) { std::cout << "Error: " << sys.lastError() << "\n"; return; }
    printPatient(*p, sys.canViewClinical());
}

static void searchPatients(MedicalSystem& sys) {
    auto res = sys.search(ask("Name contains"));
    if (res.empty()) std::cout << "No matching patients you are authorized to see.\n";
    for (const auto* p : res) std::cout << p->summary() << "\n";
}

static void addRecord(MedicalSystem& sys) {
    int id = askInt("Patient ID");
    std::string dx = ask("Diagnosis"), tx = ask("Treatment"), notes = ask("Notes");
    report(sys.addRecord(id, dx, tx, notes), sys, "Clinical record added.");
}

static void addLab(MedicalSystem& sys) {
    int id = askInt("Patient ID");
    std::string test = ask("Test name"), value = ask("Value"), unit = ask("Unit");
    report(sys.addLab(id, test, value, unit), sys, "Lab result added.");
}

static void grantAccess(MedicalSystem& sys) {
    int id = askInt("Patient ID");
    std::string u = ask("Username to authorize");
    report(sys.grantAccess(id, u), sys, "Access granted.");
}

static void createInvoice(MedicalSystem& sys) {
    int id = askInt("Patient ID");
    std::string desc = ask("Description");
    long long cents = askCents("Amount (e.g. 120.50)");
    int inv = sys.createInvoice(id, desc, cents);
    if (inv) std::cout << "Invoice #" << inv << " created.\n";
    else std::cout << "Error: " << sys.lastError() << "\n";
}

static void recordPayment(MedicalSystem& sys) {
    int id = askInt("Patient ID"), inv = askInt("Invoice ID");
    long long cents = askCents("Payment amount");
    std::string method = ask("Method (cash/card/insurance)"), card;
    if (method != "cash" && method != "card" && method != "insurance") {
        std::cout << "Error: unknown payment method.\n"; return;
    }
    if (method == "card") card = ask("Card number (only last 4 digits are kept)");
    report(sys.recordPayment(id, inv, cents, method, card), sys, "Payment recorded.");
    std::fill(card.begin(), card.end(), '\0');   // wipe card number from memory
}

static void addUser(MedicalSystem& sys) {
    std::string u = ask("New username"), full = ask("Full name"), pw = ask("Password (min 6 chars)"),
                r = ask("Role (Admin/Doctor/Nurse/Billing)");
    if (r != "Admin" && r != "Doctor" && r != "Nurse" && r != "Billing") {
        std::cout << "Error: invalid role.\n"; return;
    }
    report(sys.addUser(u, pw, full, roleFromString(r)), sys, "User created.");
}

// ---------- menus ----------
static void mainMenu(MedicalSystem& sys, const std::string& passphrase) {
    while (sys.loggedIn()) {
        std::cout << "\n--- Medical Information System | " << sys.currentUser()->fullName << " ["
                  << roleToString(sys.role()) << "] ---\n"
                  << " 1) Register patient\n 2) View patient\n 3) Search patients\n"
                  << " 4) Add clinical record\n 5) Add lab result\n 6) Create invoice\n"
                  << " 7) Record payment\n 8) Grant a user access to a patient\n";
        if (sys.canManageUsers()) std::cout << " 9) Add user\n10) View audit log\n11) List users\n";
        std::cout << " 0) Save & logout\n";
        int c = askInt("Choice");
        switch (c) {
            case 1: registerPatient(sys); break;
            case 2: viewPatient(sys); break;
            case 3: searchPatients(sys); break;
            case 4: addRecord(sys); break;
            case 5: addLab(sys); break;
            case 6: createInvoice(sys); break;
            case 7: recordPayment(sys); break;
            case 8: grantAccess(sys); break;
            case 9: if (sys.canManageUsers()) addUser(sys); break;
            case 10: if (sys.canManageUsers()) for (const auto& l : sys.auditLog()) std::cout << l << "\n"; break;
            case 11: if (sys.canManageUsers()) for (const auto& l : sys.userList()) std::cout << l << "\n"; break;
            case 0: sys.logout(); break;
            default: std::cout << "Invalid choice.\n";
        }
        if (c != 0 && !sys.saveToFile(DB_FILE, passphrase))
            std::cout << "WARNING: could not save database!\n";
    }
    if (!sys.saveToFile(DB_FILE, passphrase)) std::cout << "WARNING: could not save database!\n";
}

int main() {
    std::cout << "=== Medical Information System ===\n";
    MedicalSystem sys;
    std::string pass = ask("Database passphrase");

    int rc = sys.loadFromFile(DB_FILE, pass);
    if (rc == 2) { std::cout << "Wrong passphrase or corrupted database.\n"; return 1; }
    if (rc == 1) {
        std::cout << "No database found - creating a new one.\nCreate the first administrator account.\n";
        while (true) {
            std::string u = ask("Admin username"), full = ask("Full name"), pw = ask("Admin password (min 6 chars)");
            if (sys.addUser(u, pw, full, Role::Admin)) break;
            std::cout << "Error: " << sys.lastError() << "\n";
        }
        sys.saveToFile(DB_FILE, pass);
    }

    while (true) {
        std::cout << "\nLogin (leave username blank to quit)\n";
        std::string u = ask("Username");
        if (u.empty()) break;
        std::string p = ask("Password");
        if (sys.login(u, p)) mainMenu(sys, pass);
        else { std::cout << "Error: " << sys.lastError() << "\n"; sys.saveToFile(DB_FILE, pass); }
    }
    std::cout << "Goodbye.\n";
    return 0;
}
