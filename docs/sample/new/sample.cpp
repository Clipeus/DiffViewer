#include <iostream>
#include <string>
#include <vector>
#include <numeric>

// Computes the sum of all elements using std::accumulate.
long long Sum(const std::vector<int>& values)
{
	return std::accumulate(values.begin(), values.end(), 0LL);
}

int main(int argc, char** argv)
{
	std::vector<int> numbers = { 1, 2, 3, 4, 5 };
	std::cout << "Sum: " << Sum(numbers) << std::endl;
	return 0;
}
