/*-------------------------------------------------------------------------------
  This file is part of generalized random forest (grf).

  grf is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  grf is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with grf. If not, see <http://www.gnu.org/licenses/>.
 #-------------------------------------------------------------------------------*/

#ifndef GRF_FILEUTILITIES_H
#define GRF_FILEUTILITIES_H


#include <vector>
#include <string>

class FileTestUtilities {
public:
  static std::vector<std::vector<double>> read_csv_file(std::string file_name);
  static void write_csv_file(std::string file_name,
                             std::vector<std::vector<double>> contents);
};


#endif //GRF_FILEUTILITIES_H
