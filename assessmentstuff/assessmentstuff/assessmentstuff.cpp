// assessmentstuff.cpp : main project file.

#include "stdafx.h"
#include <iostream>
#include <vector>
#include <string>
using namespace std;

bool InGroup(vector<string> group, string s) {
	for (auto g : group) {
		if (g == s) return true;
	}

	return false;
}

vector<string> GetLargestGroup(vector<pair<string, string>> itemAssociations) {
	vector<vector<string>> itemGroups;

	for (auto ip : itemAssociations) {
		bool pairInGroup = false;
		for (vector<string>& group : itemGroups) {
			bool firstInGroup = InGroup(group, ip.first);
			bool secondInGroup = InGroup(group, ip.second);
			if (firstInGroup && secondInGroup) {
				pairInGroup = true;
			}
			else if (firstInGroup) {
				group.push_back(ip.second);
				pairInGroup = true;
			}
			else if (secondInGroup) {
				group.push_back(ip.first);
				pairInGroup = true;
			}
		}

		if (!pairInGroup) {
			vector<string> newGroup;
			newGroup.push_back(ip.first);
			newGroup.push_back(ip.second);
			itemGroups.push_back(newGroup);
		}
	}

	int max = 0;
	int maxIndex = 0;
	for (auto i = 0u; i < itemGroups.size(); ++i) {
		int s = itemGroups[i].size();
		if (s > max) {
			max = s;
			maxIndex = i;
		}
	}

	return itemGroups[maxIndex];
}

int main() {
	vector < pair<string, string>> itemAssociations;
	pair<string, string> pair1("item1", "item2");
	pair<string, string> pair2("item3", "item4");
	pair<string, string> pair3("item4", "item5");
	itemAssociations.push_back(pair1);
	itemAssociations.push_back(pair2);
	itemAssociations.push_back(pair3);
	
	vector<string> biggestGroup = GetLargestGroup(itemAssociations);
	for (auto s : biggestGroup) {
		cout << s << endl;
	}

	int n;
	cin >> n;
	return 0;
}
