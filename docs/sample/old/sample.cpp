#include <iostream>
#include <string>
#include <vector>

// Computes the sum of all elements.
int Sum(const std::vector<int>& values)
{
	int total = 0;
	for (int v : values)
		total += v;
	return total;
}

int main()
{
	std::vector<int> numbers = { 1, 2, 3, 4 };
	std::cout << "Sum: " << Sum(numbers) << std::endl;
	std::cout << "Done" << std::endl;
	return 0;
}
