#pragma once
#include "Models.h"
#include "Security.h"
#include <map>
#include <algorithm>
#include <cctype>
#include <fstream>

// ---------- serialization helpers ----------
inline std::string esc(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '\\') o += "\\\\";
        else if (c == '|') o += "\\p";
        else if (c == '\n') o += "\\n";
        else if (c == '\r') {}
        else o += c;
    }
    return o;
}
inline std::string unesc(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char n = s[++i];
            o += (n == 'p') ? '|' : (n == 'n') ? '\n' : '\\';
        } else o += s[i];
    }
    return o;
}
inline std::vector<std::string> splitFields(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == '|') { out.push_back(unesc(cur)); cur.clear(); }
        else cur += c;
    }
    out.push_back(unesc(cur));
    return out;
}

inline bool luhnValid(const std::string& n) {
    int sum = 0, count = 0;
    bool alt = false;
    for (int i = static_cast<int>(n.size()) - 1; i >= 0; --i) {
        if (!std::isdigit(static_cast<unsigned char>(n[i]))) return false;
        int d = n[i] - '0';
        if (alt) { d *= 2; if (d > 9) d -= 9; }
        sum += d; alt = !alt; ++count;
    }
    return count >= 12 && count <= 19 && sum % 10 == 0;
}

struct User {
    std::string username, salt, hash, fullName;
    Role role = Role::Nurse;
};

class MedicalSystem {
public:
    // ----- authentication -----
    bool hasUsers() const { return !users_.empty(); }
    bool loggedIn() const { return !current_.empty(); }
    const std::string& lastError() const { return err_; }

    bool addUser(const std::string& username, const std::string& password,
                 const std::string& fullName, Role role) {
        if (username.empty() || password.size() < 6) {
            err_ = "Username is required and password must be at least 6 characters.";
            return false;
        }
        if (users_.count(username)) { err_ = "Username already exists."; return false; }
        User u;
        u.username = username; u.fullName = fullName; u.role = role;
        u.salt = security::randomHex(8);
        u.hash = security::hashPassword(password, u.salt);
        users_[username] = u;
        log("User created: " + username + " (" + roleToString(role) + ")");
        return true;
    }

    bool login(const std::string& username, const std::string& password) {
        auto it = users_.find(username);
        if (it == users_.end() ||
            security::hashPassword(password, it->second.salt) != it->second.hash) {
            log("Failed login attempt for '" + username + "'");
            err_ = "Invalid username or password.";
            return false;
        }
        current_ = username;
        log("Login");
        return true;
    }

    void logout() { if (loggedIn()) log("Logout"); current_.clear(); }

    const User* currentUser() const {
        auto it = users_.find(current_);
        return it == users_.end() ? nullptr : &it->second;
    }
    Role role() const { return currentUser()->role; }

    // ----- permissions by role -----
    bool canManageUsers()   const { return loggedIn() && role() == Role::Admin; }
    bool canRegister()      const { return loggedIn() && role() != Role::Billing; }
    bool canWriteClinical() const { return loggedIn() && (role() == Role::Admin || role() == Role::Doctor); }
    bool canWriteLab()      const { return loggedIn() && (role() != Role::Billing); }
    bool canBill()          const { return loggedIn() && (role() == Role::Admin || role() == Role::Billing); }
    bool canViewClinical()  const { return loggedIn() && role() != Role::Billing; }

    // ----- patients -----
    int registerPatient(const std::string& name, const std::string& dob, const std::string& gender,
                        const std::string& phone, const std::string& address,
                        const std::string& blood, const std::vector<std::string>& allergies) {
        if (!canRegister()) { err_ = "Your role cannot register patients."; return 0; }
        if (name.empty()) { err_ = "Name is required."; return 0; }
        int id = nextPatient_++;
        Patient p(id, name, dob, gender, phone, address, blood);
        for (const auto& a : allergies) if (!a.empty()) p.addAllergy(a);
        p.authorize(current_);
        patients_[id] = p;
        log("Registered patient #" + std::to_string(id));
        return id;
    }

    // Returns nullptr if the patient doesn't exist OR the user is not authorized
    // (same message for both so patient existence is not leaked).
    Patient* getPatient(int id) {
        if (!loggedIn()) { err_ = "Not logged in."; return nullptr; }
        auto it = patients_.find(id);
        if (it == patients_.end() || !hasAccess(it->second)) {
            if (it != patients_.end()) log("ACCESS DENIED to patient #" + std::to_string(id));
            err_ = "Patient not found or access denied.";
            return nullptr;
        }
        log("Viewed patient #" + std::to_string(id));
        return &it->second;
    }

    std::vector<const Patient*> search(const std::string& term) const {
        std::vector<const Patient*> out;
        std::string t = lower(term);
        for (const auto& kv : patients_)
            if (hasAccess(kv.second) && lower(kv.second.name()).find(t) != std::string::npos)
                out.push_back(&kv.second);
        return out;
    }

    bool grantAccess(int patientId, const std::string& username) {
        Patient* p = getPatient(patientId);
        if (!p) return false;
        if (!users_.count(username)) { err_ = "No such user."; return false; }
        p->authorize(username);
        log("Granted '" + username + "' access to patient #" + std::to_string(patientId));
        return true;
    }

    bool addRecord(int id, const std::string& diagnosis, const std::string& treatment,
                   const std::string& notes) {
        if (!canWriteClinical()) { err_ = "Only doctors/admins can add clinical records."; return false; }
        Patient* p = getPatient(id);
        if (!p) return false;
        p->addRecord({today(), diagnosis, treatment, notes, currentUser()->fullName});
        log("Added clinical record to patient #" + std::to_string(id));
        return true;
    }

    bool addLab(int id, const std::string& test, const std::string& value, const std::string& unit) {
        if (!canWriteLab()) { err_ = "Your role cannot add lab results."; return false; }
        Patient* p = getPatient(id);
        if (!p) return false;
        p->addLab({test, value, unit, today()});
        log("Added lab result to patient #" + std::to_string(id));
        return true;
    }

    // ----- billing -----
    int createInvoice(int id, const std::string& description, long long cents) {
        if (!canBill()) { err_ = "Only billing/admin can create invoices."; return 0; }
        if (cents <= 0) { err_ = "Amount must be positive."; return 0; }
        Patient* p = getPatient(id);
        if (!p) return 0;
        Invoice inv; inv.id = nextInvoice_++; inv.description = description; inv.amountCents = cents;
        p->addInvoice(inv);
        log("Created invoice #" + std::to_string(inv.id) + " for patient #" + std::to_string(id));
        return inv.id;
    }

    // Card numbers are validated (Luhn) and immediately reduced to the last 4 digits.
    bool recordPayment(int id, int invoiceId, long long cents, const std::string& method,
                       std::string cardNumber) {
        if (!canBill()) { err_ = "Only billing/admin can record payments."; return false; }
        Patient* p = getPatient(id);
        if (!p) return false;
        Invoice* inv = p->findInvoice(invoiceId);
        if (!inv) { err_ = "Invoice not found."; return false; }
        if (cents <= 0) { err_ = "Amount must be positive."; return false; }
        if (cents > inv->balanceCents()) {
            err_ = "Payment exceeds outstanding balance of " + money(inv->balanceCents()) + ".";
            return false;
        }
        Payment pay; pay.id = nextPayment_; pay.amountCents = cents; pay.method = method; pay.date = today();
        if (method == "card") {
            cardNumber.erase(std::remove_if(cardNumber.begin(), cardNumber.end(),
                             [](unsigned char c) { return std::isspace(c) || c == '-'; }), cardNumber.end());
            if (!luhnValid(cardNumber)) { err_ = "Invalid card number."; return false; }
            pay.maskedRef = "**** " + cardNumber.substr(cardNumber.size() - 4);
        } else {
            pay.maskedRef = "-";
        }
        ++nextPayment_;
        inv->payments.push_back(pay);
        log("Payment #" + std::to_string(pay.id) + " of " + money(cents) + " on invoice #" +
            std::to_string(invoiceId));
        return true;
    }

    const std::vector<std::string>& auditLog() const { return audit_; }
    std::vector<std::string> userList() const {
        std::vector<std::string> v;
        for (const auto& kv : users_)
            v.push_back(kv.first + " - " + kv.second.fullName + " [" + roleToString(kv.second.role) + "]");
        return v;
    }

    // ----- persistence (plain text form; wrapped by encryption in Storage) -----
    std::string serialize() const {
        std::ostringstream o;
        o << "N|" << nextPatient_ << "|" << nextInvoice_ << "|" << nextPayment_ << "\n";
        for (const auto& kv : users_) {
            const User& u = kv.second;
            o << "S|" << esc(u.username) << "|" << u.salt << "|" << u.hash << "|"
              << roleToString(u.role) << "|" << esc(u.fullName) << "\n";
        }
        for (const auto& kv : patients_) {
            const Patient& p = kv.second;
            o << "P|" << p.id() << "|" << esc(p.name()) << "|" << esc(p.dob()) << "|" << esc(p.gender())
              << "|" << esc(p.phone()) << "|" << esc(p.address()) << "|" << esc(p.bloodType()) << "\n";
            for (const auto& a : p.allergies()) o << "A|" << esc(a) << "\n";
            for (const auto& u : p.authorized()) o << "U|" << esc(u) << "\n";
            for (const auto& r : p.records())
                o << "R|" << esc(r.date) << "|" << esc(r.diagnosis) << "|" << esc(r.treatment) << "|"
                  << esc(r.notes) << "|" << esc(r.doctor) << "\n";
            for (const auto& l : p.labs())
                o << "L|" << esc(l.test) << "|" << esc(l.value) << "|" << esc(l.unit) << "|" << esc(l.date) << "\n";
            for (const auto& i : p.invoices()) {
                o << "I|" << i.id << "|" << esc(i.description) << "|" << i.amountCents << "\n";
                for (const auto& y : i.payments)
                    o << "Y|" << i.id << "|" << y.id << "|" << y.amountCents << "|" << esc(y.method)
                      << "|" << esc(y.maskedRef) << "|" << esc(y.date) << "\n";
            }
        }
        for (const auto& g : audit_) o << "G|" << esc(g) << "\n";
        return o.str();
    }

    bool deserialize(const std::string& text) {
        users_.clear(); patients_.clear(); audit_.clear();
        std::istringstream in(text);
        std::string line;
        Patient* cur = nullptr;
        try {
            while (std::getline(in, line)) {
                if (line.empty()) continue;
                auto f = splitFields(line);
                const std::string& t = f[0];
                if (t == "N") {
                    nextPatient_ = std::stoi(f.at(1)); nextInvoice_ = std::stoi(f.at(2)); nextPayment_ = std::stoi(f.at(3));
                } else if (t == "S") {
                    User u; u.username = f.at(1); u.salt = f.at(2); u.hash = f.at(3);
                    u.role = roleFromString(f.at(4)); u.fullName = f.at(5);
                    users_[u.username] = u;
                } else if (t == "P") {
                    int id = std::stoi(f.at(1));
                    patients_[id] = Patient(id, f.at(2), f.at(3), f.at(4), f.at(5), f.at(6), f.at(7));
                    cur = &patients_[id];
                } else if (!cur && t != "G") {
                    return false;
                } else if (t == "A") cur->addAllergy(f.at(1));
                else if (t == "U") cur->authorize(f.at(1));
                else if (t == "R") cur->addRecord({f.at(1), f.at(2), f.at(3), f.at(4), f.at(5)});
                else if (t == "L") cur->addLab({f.at(1), f.at(2), f.at(3), f.at(4)});
                else if (t == "I") {
                    Invoice i; i.id = std::stoi(f.at(1)); i.description = f.at(2); i.amountCents = std::stoll(f.at(3));
                    cur->addInvoice(i);
                } else if (t == "Y") {
                    Invoice* inv = cur->findInvoice(std::stoi(f.at(1)));
                    if (!inv) return false;
                    Payment y; y.id = std::stoi(f.at(2)); y.amountCents = std::stoll(f.at(3));
                    y.method = f.at(4); y.maskedRef = f.at(5); y.date = f.at(6);
                    inv->payments.push_back(y);
                } else if (t == "G") audit_.push_back(f.at(1));
            }
        } catch (...) { return false; }
        return true;
    }

    bool saveToFile(const std::string& path, const std::string& passphrase) const {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << security::encrypt(serialize(), passphrase);
        return static_cast<bool>(out);
    }

    // Returns: 0 = ok, 1 = file missing, 2 = wrong passphrase / corrupt file
    int loadFromFile(const std::string& path, const std::string& passphrase) {
        std::ifstream in(path, std::ios::binary);
        if (!in) return 1;
        std::string blob((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        std::string plain;
        if (!security::decrypt(blob, passphrase, plain)) return 2;
        return deserialize(plain) ? 0 : 2;
    }

private:
    static std::string lower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
        return s;
    }
    bool hasAccess(const Patient& p) const {
        return loggedIn() && (role() == Role::Admin || p.isAuthorized(current_));
    }
    void log(const std::string& msg) {
        audit_.push_back(today() + " [" + (current_.empty() ? "system" : current_) + "] " + msg);
    }

    std::map<std::string, User> users_;
    std::map<int, Patient> patients_;
    std::vector<std::string> audit_;
    int nextPatient_ = 1, nextInvoice_ = 1, nextPayment_ = 1;
    std::string current_, err_;
};
