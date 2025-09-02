#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <algorithm>
#include "json.hpp"

using json = nlohmann::json;
using namespace std;

struct TimeSlot {
    string day;
    double start;
    double end;
};

struct Course {
    string code;
    string group;
    bool closed;
    int credits;
    vector<TimeSlot> times;
};

bool conflict(const vector<TimeSlot>& a, const vector<TimeSlot>& b) {
    for (const auto& t1 : a) {
        for (const auto& t2 : b) {
            if (t1.day == t2.day &&
                max(t1.start, t2.start) < min(t1.end, t2.end)) {
                return true;
            }
        }
    }
    return false;
}

// Change bestCombinations to multimap: totalCredits -> combo, sorted descending
void findBestCombinations(
    const vector<vector<Course>>& groupedCourses,
    vector<Course>& current,
    int index,
    multimap<int, vector<Course>, greater<int>>& bestCombinations,
    bool includeClosed,
    int maxOptions
) {
    if (index >= (int)groupedCourses.size()) {
        int total = 0;
        for (auto& c : current) total += c.credits;

        // Insert combo with total credits as key
        bestCombinations.insert({total, current});

        // Keep only top maxOptions combos (remove lower credit combos if too many)
        while ((int)bestCombinations.size() > maxOptions) {
            // erase last element (lowest credits because multimap sorted descending)
            auto lastIt = prev(bestCombinations.end());
            bestCombinations.erase(lastIt);
        }
        return;
    }

    for (const auto& course : groupedCourses[index]) {
        if (!includeClosed && course.closed) continue;

        bool hasConflict = false;
        for (const auto& selected : current) {
            if (conflict(course.times, selected.times)) {
                hasConflict = true;
                break;
            }
        }

        if (!hasConflict) {
            current.push_back(course);
            findBestCombinations(groupedCourses, current, index + 1, bestCombinations, includeClosed, maxOptions);
            current.pop_back();
        }
    }

    // Also try skipping this course group entirely
    findBestCombinations(groupedCourses, current, index + 1, bestCombinations, includeClosed, maxOptions);
}

void writeAllOptionsToFile(const multimap<int, vector<Course>, greater<int>>& options, const string& filename, bool includeClosed) {
    ofstream outFile(filename);
    if (!outFile) {
        cerr << "❌ Failed to open output file: " << filename << endl;
        return;
    }

    outFile << "Best options (" << (includeClosed ? "with" : "without") << " closed groups), sorted by total credits:\n\n";
    int optionNum = 1;
    for (const auto& [totalCredits, courses] : options) {
        outFile << "Option " << optionNum++ << " (Total Credits = " << totalCredits << "):\n";
        for (const auto& course : courses) {
            outFile << "  Course: " << course.code;
            if (!course.group.empty()) outFile << " (Group " << course.group << ")";
            outFile << ", Credits: " << course.credits << "\n";
            for (const auto& t : course.times) {
                outFile << "    - " << t.day << ": " << t.start << " to " << t.end << "\n";
            }
        }
        outFile << "\n";
    }

    outFile.close();
    cout << "✅ Output written to: " << filename << endl;
}

json loadSettings(const string& filename) {
    ifstream inFile(filename);
    if (!inFile) {
        cerr << "❌ Failed to open settings file: " << filename << endl;
        exit(1);
    }

    json settings;
    inFile >> settings;
    return settings;
}

vector<Course> loadCourses(const string& filename) {
    vector<Course> courses;
    ifstream inFile(filename);
    if (!inFile) {
        cerr << "❌ Failed to open course file: " << filename << endl;
        return courses;
    }

    json data;
    inFile >> data;
    for (const auto& item : data) {
        Course c;
        c.code = item.at("code").get<string>();
        c.group = item.value("group", "");
        c.closed = item.value("closed", false);
        c.credits = item.at("credits").get<int>();

        for (const auto& t : item.at("times")) {
            c.times.push_back({t.at("day").get<string>(), t.at("start").get<double>(), t.at("end").get<double>()});
        }

        courses.push_back(c);
    }

    return courses;
}

int main() {
    json settings = loadSettings("settings.json");
    vector<Course> allCourses = loadCourses("courses.json");

    int maxOptions = settings.value("maxOptions", 10);
    string outClosed = settings.value("outputWithClosed", "uniminmaxWclosed.txt");
    string outOpen = settings.value("outputWithoutClosed", "uniminmax.txt");

    map<string, vector<Course>> courseMap;
    for (auto& course : allCourses) {
        courseMap[course.code].push_back(course);
    }

    vector<vector<Course>> groupedCourses;
    for (auto& [_, group] : courseMap) {
        groupedCourses.push_back(group);
    }

    multimap<int, vector<Course>, greater<int>> bestWithClosed;
    vector<Course> current;
    findBestCombinations(groupedCourses, current, 0, bestWithClosed, true, maxOptions);

    multimap<int, vector<Course>, greater<int>> bestWithoutClosed;
    findBestCombinations(groupedCourses, current, 0, bestWithoutClosed, false, maxOptions);

    writeAllOptionsToFile(bestWithClosed, outClosed, true);
    writeAllOptionsToFile(bestWithoutClosed, outOpen, false);

    return 0;
}
