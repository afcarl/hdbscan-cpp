#pragma once
#include<string>
#include<vector>
namespace example {
	class example
	{
	private:
		string fileName;

	public:
		example(string readFileName) {
			fileName = readFileName;
		}
		void loadCsv(int number_of_values, bool skip_header);

	};
}