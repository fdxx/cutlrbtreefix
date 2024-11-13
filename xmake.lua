set_languages("cxx17")
set_arch("x86")
option("SMPATH")
option("HL2SDKPATH")
option("HL2SDKNAME")
option("SAFETYHOOKPATH")
option("DEBUG")

target("safetyhook")
    set_kind("static")
	add_files(
		"$(SAFETYHOOKPATH)/src/*.cpp",
		"$(SAFETYHOOKPATH)/zydis/Zydis.c")

	add_includedirs(
		"$(SAFETYHOOKPATH)/include",
		"$(SAFETYHOOKPATH)/zydis")

	if is_plat("windows") then
		set_toolchains("msvc")
		add_cxflags("/W3", "/w14640", "/wd4819", "/Ox", "/Oy-", "/EHsc", "/MT", "/Z7")
	else
		set_toolchains("clang")
		add_cxflags(
			"-Wall", "-Wextra",
			"-Wshadow",
			"-Wnon-virtual-dtor",
			"-Wno-unused-const-variable",
			"-Wno-unused-function",
			"-fvisibility=hidden", 
			"-fvisibility-inlines-hidden", 
			"-fPIC", "-O3", "-g3")
	end

target("cutlrbtreefix")
	set_kind("shared")
	set_prefixname("")
	set_suffixname(".ext.2.$(HL2SDKNAME)")
	add_deps("safetyhook")
	
	add_files(
		"extension/main.cpp",
		"$(SMPATH)/public/smsdk_ext.cpp")

	add_includedirs(
		"extension",
		"$(SAFETYHOOKPATH)/include",
		"$(SAFETYHOOKPATH)/zydis",
		"$(SMPATH)/public",
		"$(SMPATH)/public/extensions",
		"$(SMPATH)/sourcepawn/include",
		"$(SMPATH)/public/amtl",
		"$(SMPATH)/public/amtl/amtl",
		"$(HL2SDKPATH)/public",
		"$(HL2SDKPATH)/public/tier0",
		"$(HL2SDKPATH)/public/tier1",
		"$(HL2SDKPATH)/public/engine",
		"$(HL2SDKPATH)/public/mathlib")

	add_defines(
		"SE_EPISODEONE=1",
		"SE_DARKMESSIAH=2",
		"SE_ORANGEBOX=3",
		"SE_BLOODYGOODTIME=4",
		"SE_EYE=5",
		"SE_CSS=6",
		"SE_HL2DM=7",
		"SE_DODS=8",
		"SE_SDK2013=9",
		"SE_PVKII=10",
		"SE_BMS=11",
		"SE_TF2=12",
		"SE_LEFT4DEAD=13",
		"SE_NUCLEARDAWN=14",
		"SE_CONTAGION=15",
		"SE_LEFT4DEAD2=16",
		"SE_ALIENSWARM=17",
		"SE_PORTAL2=18",
		"SE_BLADE=19",
		"SE_INSURGENCY=20",
		"SE_DOI=21",
		"SE_MCV=22",
		"SE_CSGO=23",
		"SE_DOTA=24",
		"SE_CS2=25",
		"SE_MOCK=26")
	
	if is_config("HL2SDKNAME", "l4d") then
    	add_defines("SOURCE_ENGINE=13")
	else
		add_defines("SOURCE_ENGINE=16")
	end

	if is_plat("windows") then
		set_toolchains("msvc")
		add_defines(
			"_WINDOWS", "WIN32", "NDEBUG",
			'_CRT_SECURE_NO_WARNINGS')
		add_cxflags("/W3", "/wd4819", "/Ox", "/Oy-", "/EHsc", "/MT", "/Z7")

		if has_config("DEBUG") then
			add_cxflags("/Od")
			add_shflags("/DEBUG")
		end

		add_shflags("/OPT:ICF", "/OPT:REF")
		add_linkdirs("$(HL2SDKPATH)/lib/public");
		add_links("tier0", "tier1", "vstdlib", "kernel32", "legacy_stdio_definitions")
		
	else
		set_toolchains("clang")
		add_defines(
			"_LINUX","POSIX",
			"NO_HOOK_MALLOC", "NO_MALLOC_OVERRIDE", "NDEBUG",
			"stricmp=strcasecmp",
			"_stricmp=strcasecmp",
			"strcmpi=strcasecmp",
			"_strnicmp=strncasecmp",
			"strnicmp=strncasecmp ",
			"_snprintf=snprintf",
			"_vsnprintf=vsnprintf",
			"_alloca=alloca")
		add_cxflags(
			"-Wall",
			"-Wshadow",
			"-Wno-implicit-int-float-conversion", 
			"-Wno-overloaded-virtual", 
			"-Wno-deprecated-register", 
			"-Wno-register",
			"-Wno-non-virtual-dtor", 
			"-Wno-expansion-to-defined",
			"-fno-strict-aliasing", 
			"-fno-exceptions", 
			"-fvisibility=hidden", 
			"-fvisibility-inlines-hidden", 
			"-fPIC", "-O3")
			if has_config("DEBUG") then
				add_cxflags("-O0", "-g3")
			end
		
		add_linkdirs("$(HL2SDKPATH)/lib/linux");
		if is_config("HL2SDKNAME", "l4d") then
			add_links("$(HL2SDKPATH)/lib/linux/tier1_i486.a", "vstdlib", "tier0")
		else
			add_links("$(HL2SDKPATH)/lib/linux/tier1_i486.a", "vstdlib_srv", "tier0_srv")
		end
		
		add_shflags("-static-libstdc++", "-static-libgcc")
	end
	
	after_build(function (target)
		--os.tryrm("release")
		os.mkdir("release/addons/sourcemod/extensions")
		os.mkdir("release/addons/sourcemod/gamedata")
		os.cp(path.join(target:targetdir(), target:filename()), "release/addons/sourcemod/extensions")
		os.cp("pawn/cutlrbtreefix.autoload", "release/addons/sourcemod/extensions")
		os.cp("pawn/cutlrbtreefix.txt", "release/addons/sourcemod/gamedata")
    end)

