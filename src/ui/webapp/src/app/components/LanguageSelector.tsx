import { useState } from "react";
import { X } from "lucide-react";
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from "./ui/select";

export function LanguageSelector() {
  const [selectedLanguage, setSelectedLanguage] = useState("uk");

  const languages = [
    { code: "uk", name: "Українська" },
    { code: "ko", name: "한국어" },
    { code: "he", name: "עברית" },
  ];

  return (
    <div className="flex items-center justify-center min-h-screen bg-gray-100">
      <div className="bg-black rounded-2xl p-8 w-80 shadow-xl relative">
        {/* Close Button */}
        <button className="absolute top-4 right-4 text-white hover:opacity-70 transition-opacity">
          <X size={24} />
        </button>

        {/* ORKA Logo */}
        <h1 className="text-white text-4xl font-bold text-center mb-8">
          ORKA
        </h1>

        {/* Language Selection */}
        <div className="space-y-4">
          {/* English Language Tab */}
          <div className="flex items-center justify-between bg-white/10 rounded-lg p-3">
            <span className="text-white">English</span>
          </div>

          {/* Other Languages Selector */}
          <div className="bg-white/10 rounded-lg">
            <Select value={selectedLanguage} onValueChange={setSelectedLanguage}>
              <SelectTrigger className="bg-transparent border-none text-white">
                <SelectValue placeholder="Оберіть мову" />
              </SelectTrigger>
              <SelectContent className="bg-gray-900 border-gray-700">
                {languages.map((lang) => (
                  <SelectItem
                    key={lang.code}
                    value={lang.code}
                    className="text-white hover:bg-white/10 focus:bg-white/10"
                  >
                    {lang.name}
                  </SelectItem>
                ))}
              </SelectContent>
            </Select>
          </div>
        </div>
      </div>
    </div>
  );
}