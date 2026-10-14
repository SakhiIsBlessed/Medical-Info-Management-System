#pragma once
#include <string>
#include <vector>
#include <set>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cmath>

enum class Role { Admin, Doctor, Nurse, Billing };

inline std::string roleToString(Role r) {
    switch (r) {
        case Role::Admin:   return "Admin";
        case Role::Doctor:  return "Doctor";
        case Role::Nurse:   return "Nurse";
        case Role::Billing: return "Billing";
    }
    return "Nurse";
}
inline Role roleFromString(const std::string& s) {
    if (s == "Admin") return Role::Admin;
    if (s == "Doctor") return Role::Doctor;
    if (s == "Billing") return Role::Billing;
    return Role::Nurse;
}

inline std::string today() {
    std::time_t t = std::time(nullptr);
    char buf[16];
    std::strftime(buf, sizeof buf, "%Y-%m-%d", std::localtime(&t));
    return buf;
}

// Money is stored in cents to avoid floating-point rounding errors.
inline std::string money(long long cents) {
    std::ostringstream o;
    o << (cents < 0 ? "-" : "") << "$" << std::llabs(cents) / 100 << "."
      << std::setw(2) << std::setfill('0') << std::llabs(cents) % 100;
    return o.str();
}

struct LabResult {
    std::string test, value, unit, date;
};

struct ClinicalRecord {
    std::string date, diagnosis, treatment, notes, doctor;
};

struct Payment {
    int id = 0;
    long long amountCents = 0;
    std::string method;     // cash | card | insurance
    std::string maskedRef;  // e.g. "**** 1111" - full card numbers are NEVER stored
    std::string date;
};

struct Invoice {
    int id = 0;
    std::string description;
    long long amountCents = 0;
    std::vector<Payment> payments;

    long long paidCents() const {
        long long s = 0;
        for (const auto& p : payments) s += p.amountCents;
        return s;
    }
    long long balanceCents() const { return amountCents - paidCents(); }
};

// ---------- Class hierarchy ----------
class Person {
public:
    Person() = default;
    Person(int id, std::string name, std::string dob, std::string gender, std::string phone)
        : id_(id), name_(std::move(name)), dob_(std::move(dob)),
          gender_(std::move(gender)), phone_(std::move(phone)) {}
    virtual ~Person() = default;

    virtual std::string summary() const {
        std::ostringstream o;
        o << "#" << id_ << "  " << name_ << "  (DOB " << dob_ << ", " << gender_ << ")";
        return o.str();
    }

    int id() const { return id_; }
    const std::string& name() const { return name_; }
    const std::string& dob() const { return dob_; }
    const std::string& gender() const { return gender_; }
    const std::string& phone() const { return phone_; }

protected:
    int id_ = 0;
    std::string name_, dob_, gender_, phone_;
};

class Patient : public Person {
public:
    Patient() = default;
    Patient(int id, std::string name, std::string dob, std::string gender,
            std::string phone, std::string address, std::string bloodType)
        : Person(id, std::move(name), std::move(dob), std::move(gender), std::move(phone)),
          address_(std::move(address)), bloodType_(std::move(bloodType)) {}

    std::string summary() const override {
        return Person::summary() + "  Blood: " + bloodType_;
    }

    const std::string& address() const { return address_; }
    const std::string& bloodType() const { return bloodType_; }

    void addAllergy(const std::string& a) { allergies_.push_back(a); }
    void addRecord(const ClinicalRecord& r) { records_.push_back(r); }
    void addLab(const LabResult& l) { labs_.push_back(l); }
    void addInvoice(const Invoice& i) { invoices_.push_back(i); }
    void authorize(const std::string& user) { authorized_.insert(user); }

    bool isAuthorized(const std::string& user) const { return authorized_.count(user) > 0; }
    Invoice* findInvoice(int id) {
        for (auto& i : invoices_) if (i.id == id) return &i;
        return nullptr;
    }

    const std::vector<std::string>& allergies() const { return allergies_; }
    const std::vector<ClinicalRecord>& records() const { return records_; }
    const std::vector<LabResult>& labs() const { return labs_; }
    const std::vector<Invoice>& invoices() const { return invoices_; }
    const std::set<std::string>& authorized() const { return authorized_; }

private:
    std::string address_, bloodType_;
    std::vector<std::string> allergies_;
    std::vector<ClinicalRecord> records_;
    std::vector<LabResult> labs_;
    std::vector<Invoice> invoices_;
    std::set<std::string> authorized_;   // usernames allowed to view this patient
};
