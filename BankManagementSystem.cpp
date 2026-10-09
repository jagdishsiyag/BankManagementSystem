/*  ============================================================
    BANK MANAGEMENT APPLICATION
    Console-based banking system using OOP + File Handling
    ------------------------------------------------------------
    OOP Concepts Demonstrated:
        * Encapsulation   -> Account class hides balance & PIN
        * Abstraction     -> Account is an abstract base class
        * Inheritance     -> SavingsAccount / CurrentAccount
        * Polymorphism    -> virtual getType(), canWithdraw(),
                             display(), serialize()
        * Composition     -> Bank "has-a" vector of Accounts

    Features : Create Account, Deposit, Withdraw, Balance Enquiry,
               Display All, Update Details, Close Account
    Storage  : accounts.txt  (pipe-delimited, persistent)
    ============================================================ */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <memory>
#include <iomanip>
#include <limits>
#include <algorithm>

using namespace std;

const string DATA_FILE = "accounts.txt";

/* ---------------- Validated Input Helpers ---------------- */

int readInt(const string& prompt) {
    int value;
    while (true) {
        cout << prompt;
        if (cin >> value) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return value;
        }
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "  [!] Invalid input. Enter a whole number.\n";
    }
}

double readDouble(const string& prompt) {
    double value;
    while (true) {
        cout << prompt;
        if (cin >> value) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return value;
        }
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "  [!] Invalid input. Enter a valid amount.\n";
    }
}

string readLine(const string& prompt) {
    string s;
    cout << prompt;
    getline(cin, s);
    return s;
}

string readPin(const string& prompt) {
    while (true) {
        string pin = readLine(prompt);
        bool ok = (pin.size() == 4) &&
                  all_of(pin.begin(), pin.end(),
                         [](char c){ return c >= '0' && c <= '9'; });
        if (ok) return pin;
        cout << "  [!] PIN must be exactly 4 digits.\n";
    }
}

/* ================= Base Class : Account ================= */

class Account {
protected:
    int    accountNumber;
    string holderName;
    string pin;
    double balance;

public:
    Account(int accNo, const string& name, const string& p, double bal)
        : accountNumber(accNo), holderName(name), pin(p), balance(bal) {}

    virtual ~Account() {}

    // --- Accessors (encapsulation) ---
    int    getAccountNumber() const { return accountNumber; }
    string getHolderName()    const { return holderName; }
    double getBalance()       const { return balance; }
    bool   verifyPin(const string& p) const { return p == pin; }

    void setHolderName(const string& n) { holderName = n; }
    void setPin(const string& p)        { pin = p; }

    // --- Pure virtual (abstraction + polymorphism) ---
    virtual string getType() const = 0;
    virtual bool   canWithdraw(double amount) const = 0;

    // --- Common business logic ---
    bool deposit(double amount) {
        if (amount <= 0) return false;
        balance += amount;
        return true;
    }

    bool withdraw(double amount) {
        if (amount <= 0) return false;
        if (!canWithdraw(amount)) return false;
        balance -= amount;
        return true;
    }

    virtual void display() const {
        cout << left
             << setw(10) << accountNumber
             << setw(24) << holderName
             << setw(10) << getType()
             << setw(14) << fixed << setprecision(2) << balance
             << '\n';
    }

    virtual string serialize() const {
        ostringstream oss;
        oss << getType() << '|' << accountNumber << '|' << holderName
            << '|' << pin << '|' << fixed << setprecision(2) << balance;
        return oss.str();
    }
};

/* ================= Derived Class : Savings ================= */

class SavingsAccount : public Account {
public:
    static constexpr double MIN_BALANCE = 500.0;

    SavingsAccount(int accNo, const string& name, const string& p, double bal)
        : Account(accNo, name, p, bal) {}

    string getType() const override { return "Savings"; }

    // Must always keep at least MIN_BALANCE in the account.
    bool canWithdraw(double amount) const override {
        return (balance - amount) >= MIN_BALANCE;
    }
};

/* ================= Derived Class : Current ================= */

class CurrentAccount : public Account {
public:
    static constexpr double OVERDRAFT_LIMIT = 1000.0;

    CurrentAccount(int accNo, const string& name, const string& p, double bal)
        : Account(accNo, name, p, bal) {}

    string getType() const override { return "Current"; }

    // Allowed to go negative up to the overdraft limit.
    bool canWithdraw(double amount) const override {
        return (balance - amount) >= -OVERDRAFT_LIMIT;
    }
};

/* ================= Class : Bank ================= */

class Bank {
private:
    vector<unique_ptr<Account>> accounts;
    string fileName;
    int nextAccountNumber;

    int findIndex(int accNo) const {
        for (size_t i = 0; i < accounts.size(); ++i)
            if (accounts[i]->getAccountNumber() == accNo)
                return static_cast<int>(i);
        return -1;
    }

    void computeNextAccountNumber() {
        int maxNo = 1000;                       // accounts start from 1001
        for (const auto& acc : accounts)
            maxNo = max(maxNo, acc->getAccountNumber());
        nextAccountNumber = maxNo + 1;
    }

    // Returns account index after successful authentication, else -1.
    int authenticate() {
        int accNo = readInt("Enter Account Number : ");
        int idx = findIndex(accNo);
        if (idx == -1) {
            cout << "  [!] Account not found.\n";
            return -1;
        }
        string pin = readLine("Enter 4-digit PIN  : ");
        if (!accounts[idx]->verifyPin(pin)) {
            cout << "  [!] Incorrect PIN. Access denied.\n";
            return -1;
        }
        return idx;
    }

public:
    explicit Bank(const string& file) : fileName(file) {
        loadFromFile();
        computeNextAccountNumber();
    }

    ~Bank() { saveToFile(); }

    /* ---------- File Handling ---------- */

    void loadFromFile() {
        accounts.clear();
        ifstream fin(fileName);
        if (!fin.is_open()) return;             // first run

        string line;
        while (getline(fin, line)) {
            if (line.empty()) continue;

            stringstream ss(line);
            string type, accStr, name, pin, balStr;

            getline(ss, type,   '|');
            getline(ss, accStr, '|');
            getline(ss, name,   '|');
            getline(ss, pin,    '|');
            getline(ss, balStr, '|');

            if (type.empty() || accStr.empty()) continue;

            try {
                int    accNo = stoi(accStr);
                double bal   = balStr.empty() ? 0.0 : stod(balStr);

                if (type == "Savings")
                    accounts.push_back(make_unique<SavingsAccount>(accNo, name, pin, bal));
                else if (type == "Current")
                    accounts.push_back(make_unique<CurrentAccount>(accNo, name, pin, bal));
            } catch (...) {
                continue;                       // skip corrupted line safely
            }
        }
        fin.close();
    }

    void saveToFile() const {
        ofstream fout(fileName, ios::trunc);
        for (const auto& acc : accounts)
            fout << acc->serialize() << '\n';
        fout.close();
    }

    size_t getAccountCount() const { return accounts.size(); }

    /* ---------- Operations ---------- */

    void createAccount() {
        cout << "\n--- Create New Account ---\n";

        string name = readLine("Enter Account Holder Name : ");
        if (name.empty()) {
            cout << "  [!] Name cannot be empty.\n";
            return;
        }

        cout << "\nSelect Account Type:\n";
        cout << "  1. Savings  (minimum balance Rs." << fixed << setprecision(2)
             << SavingsAccount::MIN_BALANCE << ")\n";
        cout << "  2. Current  (overdraft allowed up to Rs."
             << CurrentAccount::OVERDRAFT_LIMIT << ")\n";

        int type = readInt("Enter choice (1-2) : ");
        if (type != 1 && type != 2) {
            cout << "  [!] Invalid account type.\n";
            return;
        }

        string pin = readPin("Set a 4-digit PIN : ");

        double initial = readDouble("Enter Initial Deposit Amount : ");
        if (initial <= 0) {
            cout << "  [!] Initial deposit must be greater than zero.\n";
            return;
        }
        if (type == 1 && initial < SavingsAccount::MIN_BALANCE) {
            cout << "  [!] Savings account needs at least Rs."
                 << SavingsAccount::MIN_BALANCE << " to open.\n";
            return;
        }

        int accNo = nextAccountNumber++;
        if (type == 1)
            accounts.push_back(make_unique<SavingsAccount>(accNo, name, pin, initial));
        else
            accounts.push_back(make_unique<CurrentAccount>(accNo, name, pin, initial));

        saveToFile();

        cout << "  [OK] Account created successfully!\n";
        cout << "       Your Account Number is : " << accNo << '\n';
        cout << "       Please remember your PIN for future transactions.\n";
    }

    void depositMoney() {
        cout << "\n--- Deposit Money ---\n";
        int idx = authenticate();
        if (idx == -1) return;

        double amount = readDouble("Enter Amount to Deposit : ");
        if (amount <= 0) {
            cout << "  [!] Deposit amount must be positive.\n";
            return;
        }

        accounts[idx]->deposit(amount);
        saveToFile();

        cout << "  [OK] Rs." << fixed << setprecision(2) << amount
             << " deposited successfully.\n";
        cout << "       New Balance : Rs." << accounts[idx]->getBalance() << '\n';
    }

    void withdrawMoney() {
        cout << "\n--- Withdraw Money ---\n";
        int idx = authenticate();
        if (idx == -1) return;

        double amount = readDouble("Enter Amount to Withdraw : ");
        if (amount <= 0) {
            cout << "  [!] Withdrawal amount must be positive.\n";
            return;
        }

        if (!accounts[idx]->withdraw(amount)) {
            cout << "  [!] Transaction failed. Insufficient funds or limit exceeded.\n";
            if (accounts[idx]->getType() == "Savings")
                cout << "       Savings accounts must keep a minimum balance of Rs."
                     << fixed << setprecision(2) << SavingsAccount::MIN_BALANCE << ".\n";
            else
                cout << "       Overdraft limit is Rs."
                     << CurrentAccount::OVERDRAFT_LIMIT << ".\n";
            return;
        }

        saveToFile();

        cout << "  [OK] Rs." << fixed << setprecision(2) << amount
             << " withdrawn successfully.\n";
        cout << "       Available Balance : Rs." << accounts[idx]->getBalance() << '\n';
    }

    void balanceEnquiry() {
        cout << "\n--- Balance Enquiry ---\n";
        int idx = authenticate();
        if (idx == -1) return;

        const Account* acc = accounts[idx].get();
        cout << "\n  --------------------------------\n";
        cout << "   Account Number : " << acc->getAccountNumber() << '\n';
        cout << "   Holder Name    : " << acc->getHolderName()    << '\n';
        cout << "   Account Type   : " << acc->getType()          << '\n';
        cout << "   Balance        : Rs." << fixed << setprecision(2)
             << acc->getBalance() << '\n';
        cout << "  --------------------------------\n";
    }

    void displayAllAccounts() const {
        cout << "\n--- All Bank Accounts ---\n";
        if (accounts.empty()) {
            cout << "  No accounts found.\n";
            return;
        }

        cout << left
             << setw(10) << "Acc No"
             << setw(24) << "Holder Name"
             << setw(10) << "Type"
             << setw(14) << "Balance" << '\n';
        cout << string(58, '-') << '\n';

        for (const auto& acc : accounts)
            acc->display();                    // polymorphic call

        cout << string(58, '-') << '\n';
        cout << "Total accounts: " << accounts.size() << '\n';
    }

    void updateAccount() {
        cout << "\n--- Update Account Details ---\n";
        int idx = authenticate();
        if (idx == -1) return;

        Account* acc = accounts[idx].get();
        cout << "  (Press Enter to keep the current value.)\n";

        string name = readLine("New Holder Name [" + acc->getHolderName() + "] : ");
        if (!name.empty()) acc->setHolderName(name);

        string changePin = readLine("Change PIN? (y/n) : ");
        if (changePin == "y" || changePin == "Y") {
            string newPin = readPin("Enter new 4-digit PIN : ");
            acc->setPin(newPin);
        }

        saveToFile();
        cout << "  [OK] Account details updated successfully.\n";
    }

    void closeAccount() {
        cout << "\n--- Close Account ---\n";
        int idx = authenticate();
        if (idx == -1) return;

        Account* acc = accounts[idx].get();
        cout << "  Account Holder : " << acc->getHolderName() << '\n';
        cout << "  Current Balance: Rs." << fixed << setprecision(2)
             << acc->getBalance() << '\n';

        if (acc->getBalance() > 0)
            cout << "  [!] Note: Please withdraw the remaining balance before closing.\n";

        string confirm = readLine("Are you sure you want to close this account? (y/n) : ");
        if (confirm != "y" && confirm != "Y") {
            cout << "  Account closure cancelled.\n";
            return;
        }

        int accNo = acc->getAccountNumber();
        accounts.erase(accounts.begin() + idx);
        saveToFile();

        cout << "  [OK] Account " << accNo << " closed successfully.\n";
    }
};

/* ================= Menu & Main ================= */

void showMenu() {
    cout << "\n=========================================\n";
    cout << "        BANK MANAGEMENT APPLICATION\n";
    cout << "=========================================\n";
    cout << "  1. Create New Account\n";
    cout << "  2. Deposit Money\n";
    cout << "  3. Withdraw Money\n";
    cout << "  4. Balance Enquiry\n";
    cout << "  5. Display All Accounts\n";
    cout << "  6. Update Account Details\n";
    cout << "  7. Close (Delete) Account\n";
    cout << "  8. Exit\n";
    cout << "=========================================\n";
}

int main() {
    Bank bank(DATA_FILE);

    cout << "Welcome to the Bank Management System!\n";
    cout << "Loaded " << bank.getAccountCount()
         << " account(s) from '" << DATA_FILE << "'.\n";

    while (true) {
        showMenu();
        int choice = readInt("Enter your choice (1-8) : ");

        switch (choice) {
            case 1: bank.createAccount();      break;
            case 2: bank.depositMoney();       break;
            case 3: bank.withdrawMoney();      break;
            case 4: bank.balanceEnquiry();     break;
            case 5: bank.displayAllAccounts(); break;
            case 6: bank.updateAccount();      break;
            case 7: bank.closeAccount();       break;
            case 8:
                cout << "\nThank you for banking with us. Goodbye!\n";
                return 0;
            default:
                cout << "  [!] Invalid choice. Please enter 1-8.\n";
        }
    }
}
