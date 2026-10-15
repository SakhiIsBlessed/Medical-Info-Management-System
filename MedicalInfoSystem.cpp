// ============================================================
//  MEDICAL INFORMATION SYSTEM  (single file, C++ only)
//  Features:
//    1. Store patient data (name, age, gender, disease)
//    2. Store lab results for each patient
//    3. Billing and payments
//    4. Login (authentication)
//    5. Data file is encrypted (so it cannot be read in Notepad)
//  Object-oriented: class Patient + class MedicalSystem
// ============================================================
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
using namespace std;

const string FILE_NAME = "patients.dat"; // file where data is saved
const char KEY = 'K';                    // secret key used for encryption
const string USERNAME = "admin";         // login details
const string PASSWORD = "1234";

// ---------------- ENCRYPTION PART ----------------
// XOR cipher: every character is mixed with the KEY.
// Doing it twice gives back the original text (encrypt = decrypt).
string xorText(string text)
{
    for (char &c : text)
        c = c ^ KEY;
    return text;
}

// Encrypted characters may look strange, so we save them as hex numbers.
string toHex(const string &text)
{
    const char *digits = "0123456789ABCDEF";
    string result;
    for (unsigned char c : text)
    {
        result += digits[c >> 4];
        result += digits[c & 15];
    }
    return result;
}

string fromHex(const string &hex)
{
    string result;
    for (size_t i = 0; i + 1 < hex.size(); i += 2)
        result += (char)stoi(hex.substr(i, 2), nullptr, 16);
    return result;
}

string encrypt(const string &text) { return toHex(xorText(text)); }
string decrypt(const string &hex) { return xorText(fromHex(hex)); }

// '|' and ';' are used to separate data in the file, so remove them from input
string cleanText(string s)
{
    for (char &c : s)
        if (c == '|' || c == ';')
            c = ' ';
    return s;
}

// ---------------- PATIENT CLASS ----------------
class Patient
{
public:
    int id;
    string name, gender, disease;
    int age;
    double bill, paid;         // total bill and amount paid so far
    vector<string> labResults; // list of lab results

    Patient() : id(0), age(0), bill(0), paid(0) {}

    void show() const
    {
        cout << "\n--------------------------------\n";
        cout << "ID      : " << id << "\n";
        cout << "Name    : " << name << "\n";
        cout << "Age     : " << age << "\n";
        cout << "Gender  : " << gender << "\n";
        cout << "Disease : " << disease << "\n";
        cout << "Lab results:\n";
        if (labResults.empty())
            cout << "   (none)\n";
        for (const string &lab : labResults)
            cout << "   - " << lab << "\n";
        cout << "Bill: " << bill << "  Paid: " << paid
             << "  Due: " << bill - paid << "\n";
        cout << "--------------------------------\n";
    }

    // Convert patient to one line of text: id|name|age|gender|disease|bill|paid|lab1;lab2;
    string toLine() const
    {
        ostringstream out;
        out << id << "|" << name << "|" << age << "|" << gender << "|" << disease
            << "|" << bill << "|" << paid << "|";
        for (const string &lab : labResults)
            out << lab << ";";
        return out.str();
    }

    // Build a patient back from a line of text
    static Patient fromLine(const string &line)
    {
        Patient p;
        vector<string> parts;
        stringstream ss(line);
        string item;
        while (getline(ss, item, '|'))
            parts.push_back(item);
        if (parts.size() < 7)
            return p; // bad line, return empty patient
        p.id = stoi(parts[0]);
        p.name = parts[1];
        p.age = stoi(parts[2]);
        p.gender = parts[3];
        p.disease = parts[4];
        p.bill = stod(parts[5]);
        p.paid = stod(parts[6]);
        if (parts.size() > 7)
        {
            stringstream labs(parts[7]);
            while (getline(labs, item, ';'))
                if (!item.empty())
                    p.labResults.push_back(item);
        }
        return p;
    }
};

// ---------------- SYSTEM CLASS ----------------
class MedicalSystem
{
private:
    vector<Patient> patients;
    int nextId;

    Patient *findPatient(int id)
    {
        for (Patient &p : patients)
            if (p.id == id)
                return &p;
        return nullptr;
    }

public:
    MedicalSystem() : nextId(1) {}

    void save()
    {
        ofstream file(FILE_NAME);
        for (const Patient &p : patients)
            file << encrypt(p.toLine()) << "\n";
    }

    void load()
    {
        ifstream file(FILE_NAME);
        string line;
        while (getline(file, line))
        {
            if (line.empty())
                continue;
            Patient p = Patient::fromLine(decrypt(line));
            if (p.id == 0)
                continue;
            patients.push_back(p);
            if (p.id >= nextId)
                nextId = p.id + 1;
        }
    }

    void addPatient()
    {
        Patient p;
        p.id = nextId++;
        cout << "Name: ";
        getline(cin, p.name);
        cout << "Age: ";
        cin >> p.age;
        cin.ignore(1000, '\n');
        cout << "Gender: ";
        getline(cin, p.gender);
        cout << "Disease: ";
        getline(cin, p.disease);
        p.name = cleanText(p.name);
        p.gender = cleanText(p.gender);
        p.disease = cleanText(p.disease);
        patients.push_back(p);
        save();
        cout << "Patient added. ID = " << p.id << "\n";
    }

    void viewAll()
    {
        if (patients.empty())
        {
            cout << "No patients yet.\n";
            return;
        }
        for (const Patient &p : patients)
            p.show();
    }

    void search()
    {
        int id;
        cout << "Enter patient ID: ";
        cin >> id;
        cin.ignore(1000, '\n');
        Patient *p = findPatient(id);
        if (p)
            p->show();
        else
            cout << "Patient not found.\n";
    }

    void addLabResult()
    {
        int id;
        cout << "Enter patient ID: ";
        cin >> id;
        cin.ignore(1000, '\n');
        Patient *p = findPatient(id);
        if (!p)
        {
            cout << "Patient not found.\n";
            return;
        }
        string lab;
        cout << "Lab result (example: Blood sugar 95 mg/dL): ";
        getline(cin, lab);
        p->labResults.push_back(cleanText(lab));
        save();
        cout << "Lab result saved.\n";
    }

    void addBill()
    {
        int id;
        double amount;
        cout << "Enter patient ID: ";
        cin >> id;
        Patient *p = findPatient(id);
        if (!p)
        {
            cin.ignore(1000, '\n');
            cout << "Patient not found.\n";
            return;
        }
        cout << "Bill amount: ";
        cin >> amount;
        cin.ignore(1000, '\n');
        if (amount <= 0)
        {
            cout << "Amount must be more than 0.\n";
            return;
        }
        p->bill += amount;
        save();
        cout << "Bill added. Total due: " << p->bill - p->paid << "\n";
    }

    void makePayment()
    {
        int id;
        double amount;
        cout << "Enter patient ID: ";
        cin >> id;
        Patient *p = findPatient(id);
        if (!p)
        {
            cin.ignore(1000, '\n');
            cout << "Patient not found.\n";
            return;
        }
        cout << "Due amount: " << p->bill - p->paid << "\nPayment amount: ";
        cin >> amount;
        cin.ignore(1000, '\n');
        if (amount <= 0 || amount > p->bill - p->paid)
        {
            cout << "Invalid payment amount.\n";
            return;
        }
        p->paid += amount;
        save();
        cout << "Payment received. Remaining due: " << p->bill - p->paid << "\n";
    }
};

// ---------------- LOGIN ----------------
bool login()
{
    // We compare the ENCRYPTED versions, so the password is never compared as plain text
    for (int attempt = 1; attempt <= 3; attempt++)
    {
        string user, pass;
        cout << "Username: ";
        getline(cin, user);
        cout << "Password: ";
        getline(cin, pass);
        if (encrypt(user) == encrypt(USERNAME) && encrypt(pass) == encrypt(PASSWORD))
            return true;
        cout << "Wrong login. Attempts left: " << 3 - attempt << "\n";
    }
    return false;
}

// ---------------- MAIN ----------------
int main()
{
    cout << "===== MEDICAL INFORMATION SYSTEM =====\n";
    if (!login())
    {
        cout << "Too many wrong attempts. Program closed.\n";
        return 0;
    }

    MedicalSystem system;
    system.load();

    int choice = -1;
    while (choice != 0)
    {
        cout << "\n1. Add patient\n2. View all patients\n3. Search patient by ID\n"
             << "4. Add lab result\n5. Add bill\n6. Make payment\n0. Exit\nChoice: ";
        if (!(cin >> choice))
        {
            cin.clear();
            cin.ignore(1000, '\n');
            choice = -1;
        }
        else
            cin.ignore(1000, '\n');

        switch (choice)
        {
        case 1:
            system.addPatient();
            break;
        case 2:
            system.viewAll();
            break;
        case 3:
            system.search();
            break;
        case 4:
            system.addLabResult();
            break;
        case 5:
            system.addBill();
            break;
        case 6:
            system.makePayment();
            break;
        case 0:
            cout << "Goodbye!\n";
            break;
        default:
            cout << "Invalid choice.\n";
        }
    }
    return 0;
}