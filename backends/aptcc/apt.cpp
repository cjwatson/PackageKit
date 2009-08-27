// apt.cc
//
//  Copyright 1999-2008 Daniel Burrows
//  Copyright (C) 2009 Daniel Nicoletti <dantti85-pk@yahoo.com.br>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; see the file COPYING.  If not, write to
//  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
//  Boston, MA 02111-1307, USA.

#include "apt.h"
#include "apt-utils.h"
#include "matcher.h"
#include "aptcc_show_broken.h"
#include "acqprogress.h"
#include "pkg_acqfile.h"

#include <apt-pkg/error.h>
#include <apt-pkg/tagfile.h>
#include <apt-pkg/algorithms.h>
#include <apt-pkg/init.h>

#include <apt-pkg/sptr.h>
#include <sys/statvfs.h>
#include <sys/statfs.h>
#define RAMFS_MAGIC     0x858458f6

#include <fstream>
#include <dirent.h>
#include <assert.h>

aptcc::aptcc(PkBackend *backend, bool &cancel, pkgSourceList &apt_source_list)
	:
	packageRecords(0),
	packageCache(0),
	packageDepCache(0),
	Map(0),
	Policy(0),
	m_backend(backend),
	_cancel(cancel),
	m_pkgSourceList(apt_source_list)
{
}

bool aptcc::init(const char *locale)
{
	// Generate it and map it
	setlocale(LC_ALL, locale);
	bool Res = pkgMakeStatusCache(m_pkgSourceList, Progress, &Map, true);
	Progress.Done();
	if(!Res) {
		return false;
		//"The package lists or status file could not be parsed or opened."
	}

	packageCache = new pkgCache(Map);
	if (_error->PendingError()) {
		return false;
	}

	// create depcache
	Policy = new pkgPolicy(packageCache);
	if (_error->PendingError()) {
		return false;
	}

	if (!ReadPinFile(*Policy)) {
		return false;
	}

	packageDepCache = new pkgDepCache(packageCache, Policy);
	if (_error->PendingError()) {
		return false;
	}

	packageDepCache->Init(&Progress);
	Progress.Done();
	if (_error->PendingError()) {
		return false;
	}

	// Create the text record parser
	packageRecords = new pkgRecords(*packageDepCache);
}

aptcc::~aptcc()
{
	if (packageRecords)
	{
		egg_debug ("~apt_init packageRecords");
		delete packageRecords;
	}

	if (packageCache)
	{
		egg_debug ("~apt_init packageCache");
		delete packageCache;
	}

	if (packageDepCache)
	{
		egg_debug ("~apt_init packageDepCache");
		delete packageDepCache;
	}

	if (Policy)
	{
		egg_debug ("~apt_init Policy");
		delete Policy;
	}

	delete Map;
}

pkgCache::VerIterator aptcc::find_candidate_ver(const pkgCache::PkgIterator &pkg)
{
	// get the candidate version iterator
	return (*packageDepCache)[pkg].CandidateVerIter(*packageDepCache);
}

pkgCache::VerIterator aptcc::find_ver(const pkgCache::PkgIterator &pkg)
{
	// if the package is installed return the current version
	if(!pkg.CurrentVer().end()) {
		return pkg.CurrentVer();
	}

	// Else get the candidate version iterator
	pkgCache::VerIterator candver = find_candidate_ver(pkg);
	if(!candver.end())
	{
		return candver;
	}

	// return the version list as a last resource
	return pkg.VersionList();
}

pkgDepCache::StateCache aptcc::get_state(const pkgCache::PkgIterator &pkg)
{
	return (*packageDepCache)[pkg];
}

bool aptcc::is_held(const pkgCache::PkgIterator &pkg)
{
//   aptitude_state state=get_ext_state(pkg);
//       pkgTagFile tagfile(&state_file);
//       pkgTagSection section;

	pkgCache::VerIterator candver = find_candidate_ver(pkg);

	return !pkg.CurrentVer().end() &&
	    (pkg->SelectedState == pkgCache::State::Hold ||
	    (!candver.end() && false/*candver.VerStr() == state.forbidver*/));
	// TODO add forbid ver support
}

void aptcc::mark_all_upgradable(bool with_autoinst,
					bool ignore_removed/*,
					undo_group *undo*/)
{
//   if(read_only && !read_only_permission())
//     {
//       if(group_level == 0)
// 	read_only_fail();
//       return;
//     }

//   pre_package_state_changed();

// //   action_group group(*this, NULL);


	for(int iter=0; iter==0 || (iter==1 && with_autoinst); ++iter) {
		// Do this twice, only turning auto-install on the second time.
		// A reason for this is the following scenario:
		//
		// Packages A and B are installed at 1.0.  Package C is not installed.
		// Version 2.0 of each package is available.
		//
		// Version 2.0 of A depends on "C (= 2.0) | B (= 2.0)".
		//
		// Upgrading A if B is not upgraded will cause this dependency to
		// break.  Auto-install will then cheerfully fulfill it by installing
		// C.
		//
		// A real-life example of this is xemacs21, xemacs21-mule, and
		// xemacs21-nomule; aptitude would keep trying to install the mule
		// version on upgrades.
		bool do_autoinstall=(iter==1);

		for(pkgCache::PkgIterator i=packageDepCache->PkgBegin(); !i.end(); i++) {
			pkgDepCache::StateCache state = get_state(i);
// 			aptitude_state &estate = get_ext_state(i);

			if(i.CurrentVer().end()){
				continue;
			}

			bool do_upgrade = false;

			if(!ignore_removed) {
			    do_upgrade = state.Status > 0 && !is_held(i);
			} else {
				switch(i->SelectedState) {
				    // This case shouldn't really happen:
				    // if this shouldn't happen i guess we don't
				    // even need to worry? am i right?
		    // 		case pkgCache::State::Unknown:
		    // 		  estate.selection_state=pkgCache::State::Install;

				    // Fall through
				case pkgCache::State::Install:
					if(state.Status > 0 && !is_held(i)) {
						do_upgrade = true;
					}
					break;
				default:
					break;
				}
			}

			if(do_upgrade) {
		// 	      pre_package_state_changed();
			    dirty = true;
			    packageDepCache->MarkInstall(i, do_autoinstall);
			}
		}
	}
}

// used to emit packages it collects all the needed info
void aptcc::emit_package(const pkgCache::PkgIterator &pkg,
			 const pkgCache::VerIterator &ver,
			 PkBitfield filters,
			 PkInfoEnum state)
{
	// check the state enum to see if it was not set.
	if (state == PK_INFO_ENUM_UNKNOWN) {
		if (pkg->CurrentState == pkgCache::State::Installed) {
			state = PK_INFO_ENUM_INSTALLED;
		} else {
			state = PK_INFO_ENUM_AVAILABLE;
		}
	}

	if (filters != 0) {
		std::string str = ver.Section();
		std::string section, repo_section;

		size_t found;
		found = str.find_last_of("/");
		section = str.substr(found + 1);
		repo_section = str.substr(0, found);

		if (pk_bitfield_contain (filters, PK_FILTER_ENUM_NOT_INSTALLED)
		    && state == PK_INFO_ENUM_INSTALLED) {
			return;
		} else if (pk_bitfield_contain (filters, PK_FILTER_ENUM_INSTALLED)
		    && state == PK_INFO_ENUM_AVAILABLE) {
			return;
		}

		if (pk_bitfield_contain (filters, PK_FILTER_ENUM_DEVELOPMENT)) {
			// if ver.end() means unknow
			// strcmp will be true when it's different than devel
			std::string pkgName = pkg.Name();
			if (!ends_with(pkgName, "-dev") &&
			    !ends_with(pkgName, "-dbg") &&
			    section.compare("devel") &&
			    section.compare("libdevel")) {
				return;
			}
		} else if (pk_bitfield_contain (filters, PK_FILTER_ENUM_NOT_DEVELOPMENT)) {
			std::string pkgName = pkg.Name();
			if (ends_with(pkgName, "-dev") ||
			    ends_with(pkgName, "-dbg") ||
			    !section.compare("devel") ||
			    !section.compare("libdevel")) {
				return;
			}
		}

		if (pk_bitfield_contain (filters, PK_FILTER_ENUM_GUI)) {
			// if ver.end() means unknow
			// strcmp will be true when it's different than x11
			if (section.compare("x11") && section.compare("gnome") &&
			    section.compare("kde") && section.compare("graphics")) {
				return;
			}
		} else if (pk_bitfield_contain (filters, PK_FILTER_ENUM_NOT_GUI)) {
			if (!section.compare("x11") || !section.compare("gnome") ||
			    !section.compare("kde") || !section.compare("graphics")) {
				return;
			}
		}

		// TODO add Ubuntu handling
		if (pk_bitfield_contain (filters, PK_FILTER_ENUM_FREE)) {
			if (!repo_section.compare("contrib") || !repo_section.compare("non-free")) {
				return;
			}
		} else if (pk_bitfield_contain (filters, PK_FILTER_ENUM_NOT_FREE)) {
			if (repo_section.compare("contrib") && repo_section.compare("non-free")) {
				return;
			}
		}

		// TODO test this one..
		if (pk_bitfield_contain (filters, PK_FILTER_ENUM_COLLECTIONS)) {
			if (!repo_section.compare("metapackages")) {
				return;
			}
		} else if (pk_bitfield_contain (filters, PK_FILTER_ENUM_NOT_COLLECTIONS)) {
			if (repo_section.compare("metapackages")) {
				return;
			}
		}

	}
	pkgCache::VerFileIterator vf = ver.FileList();

	gchar *package_id;
	package_id = pk_package_id_build(pkg.Name(),
					 ver.VerStr(),
					 ver.Arch(),
					 vf.File().Archive());
	pk_backend_package(m_backend,
			   state,
			   package_id,
			   get_short_description(ver, packageRecords).c_str());
}

void aptcc::emit_packages(vector<pair<pkgCache::PkgIterator, pkgCache::VerIterator> > &output,
			  PkBitfield filters)
{
	for(vector<pair<pkgCache::PkgIterator, pkgCache::VerIterator> >::iterator i=output.begin();
	    i != output.end(); ++i)
	{
		if (_cancel) {
			break;
		}
		emit_package(i->first, i->second, filters);
	}
}

// used to emit packages it collects all the needed info
void aptcc::emit_details(const pkgCache::PkgIterator &pkg)
{
	pkgCache::VerIterator ver = find_ver(pkg);
	std::string section = ver.Section();

	size_t found;
	found = section.find_last_of("/");
	section = section.substr(found + 1);

	pkgCache::VerFileIterator vf = ver.FileList();
	pkgRecords::Parser &rec = packageRecords->Lookup(vf);

	std::string homepage;
// TODO support this
// #ifdef APT_HAS_HOMEPAGE
	if(rec.Homepage() != "") {
		homepage = rec.Homepage();
	}
// #endif

	gchar *package_id;
	package_id = pk_package_id_build(pkg.Name(),
					ver.VerStr(),
					ver.Arch(),
					vf.File().Archive());
	pk_backend_details(m_backend,
			   package_id,
			   "unknown",
			   get_enum_group(section),
			   get_long_description_parsed(ver, packageRecords).c_str(),
			   homepage.c_str(),
			   ver->Size);
}

// used to emit packages it collects all the needed info
void aptcc::emit_update_detail(const pkgCache::PkgIterator &pkg)
{
	pkgCache::VerIterator candver = find_candidate_ver(pkg);

	pkgCache::VerFileIterator vf = candver.FileList();
	pkgRecords::Parser &rec = packageRecords->Lookup(vf);
	string archive(vf.File().Archive());
	gchar *package_id;
	package_id = pk_package_id_build(pkg.Name(),
					candver.VerStr(),
					candver.Arch(),
					archive.c_str());

	pkgCache::VerIterator currver = find_ver(pkg);
	pkgCache::VerFileIterator currvf = currver.FileList();
	gchar *current_package_id;
	current_package_id = pk_package_id_build(pkg.Name(),
						currver.VerStr(),
						currver.Arch(),
						currvf.File().Archive());

	PkUpdateStateEnum updateState = PK_UPDATE_STATE_ENUM_UNKNOWN;
	if (archive.compare("stable") == 0) {
		updateState = PK_UPDATE_STATE_ENUM_STABLE;
	} else if (archive.compare("testing") == 0) {
		updateState = PK_UPDATE_STATE_ENUM_TESTING;
	} else if (archive.compare("unstable")  == 0 ||
		archive.compare("experimental") == 0)
	{
		updateState = PK_UPDATE_STATE_ENUM_UNSTABLE;
	}
	pk_backend_update_detail(m_backend,
				 package_id,
				 current_package_id,//const gchar *updates
				 "",//const gchar *obsoletes
				 "",//const gchar *vendor_url
				 "",//const gchar *bugzilla_url
				 "",//const gchar *cve_url
				 PK_RESTART_ENUM_NONE,//PkRestartEnum restart
				 "",//const gchar *update_text
				 "",//const gchar *changelog
				 updateState,//PkUpdateStateEnum state
				 "",//const gchar *issued_text
				 ""//const gchar *updated_text
				 );
}

void aptcc::get_depends(vector<pair<pkgCache::PkgIterator, pkgCache::VerIterator> > &output,
			pkgCache::PkgIterator pkg,
			bool recursive)
{
	pkgCache::DepIterator dep = find_ver(pkg).DependsList();
	while (!dep.end()) {
		if (_cancel) {
			break;
		}
		pkgCache::VerIterator ver = find_ver(dep.TargetPkg());
		// Ignore packages that exist only due to dependencies.
		if (ver.end()) {
			dep++;
			continue;
		} else if (dep->Type == pkgCache::Dep::Depends) {
			if (recursive) {
				if (!contains(output, dep.TargetPkg())) {
					output.push_back(pair<pkgCache::PkgIterator, pkgCache::VerIterator>(dep.TargetPkg(), ver));
					get_depends(output, dep.TargetPkg(), recursive);
				}
			} else {
				output.push_back(pair<pkgCache::PkgIterator, pkgCache::VerIterator>(dep.TargetPkg(), ver));
			}
		}
		dep++;
	}
}

void aptcc::get_requires(vector<pair<pkgCache::PkgIterator, pkgCache::VerIterator> > &output,
			pkgCache::PkgIterator pkg,
			bool recursive)
{
	for (pkgCache::PkgIterator parentPkg = packageCache->PkgBegin(); !parentPkg.end(); ++parentPkg) {
		if (_cancel) {
			break;
		}
		// Ignore packages that exist only due to dependencies.
		if (parentPkg.VersionList().end() && parentPkg.ProvidesList().end()) {
			continue;
		}

		// Don't insert virtual packages instead add what it provides
		pkgCache::VerIterator ver = find_ver(parentPkg);
		if (ver.end() == false) {
			vector<pair<pkgCache::PkgIterator, pkgCache::VerIterator> > deps;
			get_depends(deps, parentPkg, false);
			for (vector<pair<pkgCache::PkgIterator, pkgCache::VerIterator> >::iterator i=deps.begin();
			    i != deps.end();
			    ++i)
			{
				if (i->first == pkg) {
					if (recursive) {
						if (!contains(output, parentPkg)) {
							output.push_back(pair<pkgCache::PkgIterator, pkgCache::VerIterator>(parentPkg, ver));
							get_requires(output, parentPkg, recursive);
						}
					} else {
						output.push_back(pair<pkgCache::PkgIterator, pkgCache::VerIterator>(parentPkg, ver));
					}
					break;
				}
			}
		}
	}
}

// used to return files it reads, using the info from the files in /var/lib/dpkg/info/
vector<string> search_file (PkBackend *backend, const string &file_name, bool &_cancel)
{
	vector<string> packageList;
	regex_t re;

	if(regcomp(&re, file_name.c_str(), REG_ICASE|REG_NOSUB) != 0) {
		egg_debug("Regex compilation error");
		return vector<string>();
	}

	DIR *dp;
	struct dirent *dirp;
	if (!(dp = opendir("/var/lib/dpkg/info/"))) {
		egg_debug ("Error opening /var/lib/dpkg/info/\n");
		regfree(&re);
		return vector<string>();
	}

	string line;
	while ((dirp = readdir(dp)) != NULL) {
		if (_cancel) {
			break;
		}
		if (ends_with(dirp->d_name, ".list")) {
			string f = "/var/lib/dpkg/info/" + string(dirp->d_name);
			ifstream in(f.c_str());
			if (!in != 0) {
				continue;
			}
			map<int, bool> matchers_used;
			while (!in.eof()) {
				getline(in, line);
				if (regexec(&re, line.c_str(), (size_t)0, NULL, 0) == 0) {
					string file(dirp->d_name);
					packageList.push_back(file.erase(file.size() - 5, file.size()));
					break;
				}
			}
		}
	}
	closedir(dp);
	regfree(&re);
	return packageList;
}

// used to emit files it reads the info directly from the files
void emit_files (PkBackend *backend, const PkPackageId *pi)
{
	static string filelist;
	string line;

	filelist.erase(filelist.begin(), filelist.end());

	string f = "/var/lib/dpkg/info/" + string(pi->name) + ".list";
	if (FileExists(f)) {
		ifstream in(f.c_str());
		if (!in != 0) {
			return;
		}
		while (in.eof() == false && filelist.empty()) {
			getline(in, line);
			filelist += line;
		}
		while (in.eof() == false) {
			getline(in, line);
			if (!line.empty()) {
				filelist += ";" + line;
			}
		}

		if (!filelist.empty()) {
			pk_backend_files (backend, pk_package_id_to_string(pi), filelist.c_str());
		}
	}
}


static bool CheckAuth(pkgAcquire& Fetcher, PkBackend *backend)
{
   string UntrustedList;
   for (pkgAcquire::ItemIterator I = Fetcher.ItemsBegin(); I < Fetcher.ItemsEnd(); ++I)
   {
      if (!(*I)->IsTrusted())
      {
         UntrustedList += string((*I)->ShortDesc()) + " ";
      }
   }

   if (UntrustedList == "")
   {
      return true;
   }

   string warning("WARNING: The following packages cannot be authenticated!\n");
   warning += UntrustedList;
   pk_backend_message(backend,
		      PK_MESSAGE_ENUM_UNTRUSTED_PACKAGE,
		      warning.c_str());

//    ShowList(c2out,_("WARNING: The following packages cannot be authenticated!"),UntrustedList,"");

   if (_config->FindB("APT::Get::AllowUnauthenticated", false) == true)
   {
      egg_debug ("Authentication warning overridden.\n");
      return true;
   }

   return false;
}

bool TryToInstall(pkgCache::PkgIterator Pkg,
		  pkgDepCache &Cache,
		  pkgProblemResolver &Fix,
		  bool Remove,
		  bool BrokenFix,
		  unsigned int &ExpectedInst,
		  bool AllowFail = true)
{
   /* This is a pure virtual package and there is a single available
      provides */
   if (Cache[Pkg].CandidateVer == 0 && Pkg->ProvidesList != 0 &&
       Pkg.ProvidesList()->NextProvides == 0)
   {
      pkgCache::PkgIterator Tmp = Pkg.ProvidesList().OwnerPkg();
      ioprintf(c1out,_("Note, selecting %s instead of %s\n"),
	       Tmp.Name(),Pkg.Name());
      Pkg = Tmp;
   }

   // Handle the no-upgrade case
   if (_config->FindB("APT::Get::upgrade",true) == false &&
       Pkg->CurrentVer != 0)
   {
      if (AllowFail == true)
	 ioprintf(c1out,_("Skipping %s, it is already installed and upgrade is not set.\n"),
		  Pkg.Name());
      return true;
   }

   // Check if there is something at all to install
   pkgDepCache::StateCache &State = Cache[Pkg];
   if (Remove == true && Pkg->CurrentVer == 0)
   {
      Fix.Clear(Pkg);
      Fix.Protect(Pkg);
      Fix.Remove(Pkg);

      /* We want to continue searching for regex hits, so we return false here
         otherwise this is not really an error. */
      if (AllowFail == false)
	 return false;

      ioprintf(c1out,_("Package %s is not installed, so not removed\n"),Pkg.Name());
      return true;
   }

   if (State.CandidateVer == 0 && Remove == false)
   {
      if (AllowFail == false)
	 return false;

      if (Pkg->ProvidesList != 0)
      {
	 ioprintf(c1out,_("Package %s is a virtual package provided by:\n"),
		  Pkg.Name());

	 pkgCache::PrvIterator I = Pkg.ProvidesList();
	 for (; I.end() == false; I++)
	 {
	    pkgCache::PkgIterator Pkg = I.OwnerPkg();

	    if (Cache[Pkg].CandidateVerIter(Cache) == I.OwnerVer())
	    {
	       if (Cache[Pkg].Install() == true && Cache[Pkg].NewInstall() == false)
		  c1out << "  " << Pkg.Name() << " " << I.OwnerVer().VerStr() <<
		  _(" [Installed]") << endl;
	       else
		  c1out << "  " << Pkg.Name() << " " << I.OwnerVer().VerStr() << endl;
	    }
	 }
	 c1out << _("You should explicitly select one to install.") << endl;
      }
      else
      {
	 ioprintf(c1out,
	 _("Package %s is not available, but is referred to by another package.\n"
	   "This may mean that the package is missing, has been obsoleted, or\n"
           "is only available from another source\n"),Pkg.Name());

	 string List;
	 string VersionsList;
	 SPtrArray<bool> Seen = new bool[Cache.Head().PackageCount];
	 memset(Seen,0,Cache.Head().PackageCount*sizeof(*Seen));
	 pkgCache::DepIterator Dep = Pkg.RevDependsList();
	 for (; Dep.end() == false; Dep++)
	 {
	    if (Dep->Type != pkgCache::Dep::Replaces)
	       continue;
	    if (Seen[Dep.ParentPkg()->ID] == true)
	       continue;
	    Seen[Dep.ParentPkg()->ID] = true;
	    List += string(Dep.ParentPkg().Name()) + " ";
        //VersionsList += string(Dep.ParentPkg().CurVersion) + "\n"; ???
	 }
	 ShowList(c1out,_("However the following packages replace it:"),List,VersionsList);
      }

      _error->Error(_("Package %s has no installation candidate"),Pkg.Name());
      return false;
   }

   Fix.Clear(Pkg);
   Fix.Protect(Pkg);
   if (Remove == true)
   {
      Fix.Remove(Pkg);
      Cache.MarkDelete(Pkg,_config->FindB("APT::Get::Purge",false));
      return true;
   }

   // Install it
   Cache.MarkInstall(Pkg,false);
   if (State.Install() == false)
   {
      if (_config->FindB("APT::Get::ReInstall",false) == true)
      {
	 if (Pkg->CurrentVer == 0 || Pkg.CurrentVer().Downloadable() == false)
	    ioprintf(c1out,_("Reinstallation of %s is not possible, it cannot be downloaded.\n"),
		     Pkg.Name());
	 else
	    Cache.SetReInstall(Pkg,true);
      }
      else
      {
	 if (AllowFail == true)
	    ioprintf(c1out,_("%s is already the newest version.\n"),
		     Pkg.Name());
      }
   }
   else
      ExpectedInst++;

   // Install it with autoinstalling enabled (if we not respect the minial
   // required deps or the policy)
   if ((State.InstBroken() == true || State.InstPolicyBroken() == true) && BrokenFix == false)
      Cache.MarkInstall(Pkg,true);

   return true;
}

									/*}}}*/

// InstallPackages - Actually download and install the packages		/*{{{*/
// ---------------------------------------------------------------------
/* This displays the informative messages describing what is going to
   happen and then calls the download routines */
bool aptcc::installPackages(pkgDepCache &Cache,
			    bool ShwKept,
			    bool Ask,
			    bool Safety)
{
	if (_config->FindB("APT::Get::Purge",false) == true)
	{
	    pkgCache::PkgIterator I = Cache.PkgBegin();
	    for (; I.end() == false; I++)
	    {
		if (I.Purge() == false && Cache[I].Mode == pkgDepCache::ModeDelete)
		    Cache.MarkDelete(I,true);
	    }
	}

	bool Fail = false;
	bool Essential = false;

// we don't show things here
	// Show all the various warning indicators
// 	ShowDel(c1out,Cache);
// 	ShowNew(c1out,Cache);
// 	if (ShwKept == true)
// 	    ShowKept(c1out,Cache);
// 	Fail |= !ShowHold(c1out,Cache);
// 	if (_config->FindB("APT::Get::Show-Upgraded",true) == true)
// 	    ShowUpgraded(c1out,Cache);
// 	Fail |= !ShowDowngraded(c1out,Cache);
// 	if (_config->FindB("APT::Get::Download-Only",false) == false)
// 		Essential = !ShowEssential(c1out,Cache);
// 	Fail |= Essential;
// 	Stats(c1out,Cache);

	// Sanity check
	if (Cache.BrokenCount() != 0)
	{
// 	    ShowBroken(c1out,Cache,false);
	    show_broken(m_backend, this);
	    return _error->Error("Internal error, InstallPackages was called with broken packages!");
	}

	if (Cache.DelCount() == 0 && Cache.InstCount() == 0 &&
	    Cache.BadCount() == 0)
	    return true;

	// No remove flag
	if (Cache.DelCount() != 0 && _config->FindB("APT::Get::Remove",true) == false)
	    return _error->Error("Packages need to be removed but remove is disabled.");

	// Run the simulator ..
	if (_config->FindB("APT::Get::Simulate") == true)
	{
	    pkgSimulate PM(&Cache);
	    int status_fd = _config->FindI("APT::Status-Fd",-1);
	    pkgPackageManager::OrderResult Res = PM.DoInstall(status_fd);
	    if (Res == pkgPackageManager::Failed)
		return false;
	    if (Res != pkgPackageManager::Completed)
		return _error->Error("Internal error, Ordering didn't finish");
	    return true;
	}

	// Create the text record parser
	pkgRecords Recs(Cache);
	if (_error->PendingError() == true)
	    return false;

	// Lock the archive directory
	FileFd Lock;
	if (_config->FindB("Debug::NoLocking",false) == false &&
	    _config->FindB("APT::Get::Print-URIs") == false)
	{
	    Lock.Fd(GetLock(_config->FindDir("Dir::Cache::Archives") + "lock"));
	    if (_error->PendingError() == true)
		return _error->Error("Unable to lock the download directory");
	}

	// Create the download object
	AcqPackageKitStatus Stat(this, m_backend, _cancel, _config->FindI("quiet",0));
	pkgAcquire Fetcher(&Stat);

	// Read the source list
// 	pkgSourceList List;
// 	if (List.ReadMainList() == false)
// 	    return _error->Error("The list of sources could not be read.");

	// Create the package manager and prepare to download
	SPtr<pkgPackageManager> PM= _system->CreatePM(&Cache);
	if (PM->GetArchives(&Fetcher, &m_pkgSourceList, &Recs) == false ||
	    _error->PendingError() == true)
	    return false;


	// Generate the list of affected packages and sort it
	for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; I++)
	{
		// Ignore no-version packages
		if (I->VersionList == 0) {
			continue;
		}

		// Not interesting
		if ((Cache[I].Keep() == true ||
		    Cache[I].InstVerIter(Cache) == I.CurrentVer()) &&
		    I.State() == pkgCache::PkgIterator::NeedsNothing &&
		    (Cache[I].iFlags & pkgDepCache::ReInstall) != pkgDepCache::ReInstall &&
		    (I.Purge() != false || Cache[I].Mode != pkgDepCache::ModeDelete ||
		    (Cache[I].iFlags & pkgDepCache::Purge) != pkgDepCache::Purge))
		{
			continue;
		}

		// Append it to the list
		Stat.addPackagePair(pair<pkgCache::PkgIterator, pkgCache::VerIterator>(I, Cache[I].InstVerIter(Cache)));
	}

	// Display statistics
	double FetchBytes = Fetcher.FetchNeeded();
	double FetchPBytes = Fetcher.PartialPresent();
	double DebBytes = Fetcher.TotalNeeded();
	if (DebBytes != Cache.DebSize())
	{
// 	    c0out << DebBytes << ',' << Cache.DebSize() << endl;
	    _error->Warning("How odd.. The sizes didn't match, email apt@packages.debian.org");
	}

	// Number of bytes
// 	if (DebBytes != FetchBytes)
// 	    ioprintf(c1out, "Need to get %sB/%sB of archives.\n",
// 		    SizeToStr(FetchBytes).c_str(),SizeToStr(DebBytes).c_str());
// 	else if (DebBytes != 0)
// 	    ioprintf(c1out, "Need to get %sB of archives.\n",
// 		    SizeToStr(DebBytes).c_str());

	// Size delta
// 	if (Cache.UsrSize() >= 0)
// 	    ioprintf(c1out, "After this operation, %sB of additional disk space will be used.\n",
// 		    SizeToStr(Cache.UsrSize()).c_str());
// 	else
// 	    ioprintf(c1out, "After this operation, %sB disk space will be freed.\n",
// 		    SizeToStr(-1*Cache.UsrSize()).c_str());

	if (_error->PendingError() == true)
	    return false;

	/* Check for enough free space */
	struct statvfs Buf;
	string OutputDir = _config->FindDir("Dir::Cache::Archives");
	if (statvfs(OutputDir.c_str(),&Buf) != 0) {
		return _error->Errno("statvfs", "Couldn't determine free space in %s",
				    OutputDir.c_str());
	}
	if (unsigned(Buf.f_bfree) < (FetchBytes - FetchPBytes)/Buf.f_bsize)
	{
		struct statfs Stat;
		if (statfs(OutputDir.c_str(), &Stat) != 0 ||
				unsigned(Stat.f_type) != RAMFS_MAGIC)
		{
			pk_backend_error_code(m_backend,
					      PK_ERROR_ENUM_NO_SPACE_ON_DEVICE,
					      string("You don't have enough free space in ").append(OutputDir).c_str());
			return _error->Error("You don't have enough free space in %s.",
					    OutputDir.c_str());
		}
	}

	// Fail safe check
// 	if (_config->FindI("quiet",0) >= 2 ||
// 	    _config->FindB("APT::Get::Assume-Yes",false) == true)
// 	{
// 	    if (Fail == true && _config->FindB("APT::Get::Force-Yes",false) == false)
// 		return _error->Error("There are problems and -y was used without --force-yes");
// 	}

	if (Essential == true && Safety == true)
	{
	    if (_config->FindB("APT::Get::Trivial-Only",false) == true)
		return _error->Error("Trivial Only specified but this is not a trivial operation.");

	    pk_backend_error_code(m_backend,
				  PK_ERROR_ENUM_TRANSACTION_ERROR,
				  "You was about to do something potentially harmful.\n"
				  "Please use aptitude or synaptic to have more information.");
	    return false;
// 	    const char *Prompt = "Yes, do as I say!";
// 	    ioprintf(c2out,
// 		    "You are about to do something potentially harmful.\n"
// 			"To continue type in the phrase '%s'\n"
// 			" ?] ",Prompt);
// 	    c2out << flush;
// 	    if (AnalPrompt(Prompt) == false)
// 	    {
// 		c2out << "Abort." << endl;
// 		exit(1);
// 	    }
	}

	// Just print out the uris an exit if the --print-uris flag was used
// 	if (_config->FindB("APT::Get::Print-URIs") == true)
// 	{
// 	    pkgAcquire::UriIterator I = Fetcher.UriBegin();
// 	    for (; I != Fetcher.UriEnd(); I++)
// 		cout << '\'' << I->URI << "' " << flNotDir(I->Owner->DestFile) << ' ' <<
// 		    I->Owner->FileSize << ' ' << I->Owner->HashSum() << endl;
// 	    return true;
// 	}

	if (!CheckAuth(Fetcher, m_backend))
	    return false;

	/* Unlock the dpkg lock if we are not going to be doing an install
	    after. */
// 	if (_config->FindB("APT::Get::Download-Only",false) == true)
// 	    _system->UnLock();

	// Run it
	while (1)
	{
	    bool Transient = false;
	    if (_config->FindB("APT::Get::Download",true) == false)
	    {
		for (pkgAcquire::ItemIterator I = Fetcher.ItemsBegin(); I < Fetcher.ItemsEnd();)
		{
		    if ((*I)->Local == true)
		    {
		    I++;
		    continue;
		    }

		    // Close the item and check if it was found in cache
		    (*I)->Finished();
		    if ((*I)->Complete == false)
		    Transient = true;

		    // Clear it out of the fetch list
		    delete *I;
		    I = Fetcher.ItemsBegin();
		}
	    }

	    if (Fetcher.Run() == pkgAcquire::Failed)
		return false;

	    // Print out errors
	    bool Failed = false;
	    for (pkgAcquire::ItemIterator I = Fetcher.ItemsBegin(); I != Fetcher.ItemsEnd(); I++)
	    {
		if ((*I)->Status == pkgAcquire::Item::StatDone &&
		    (*I)->Complete == true)
		    continue;

		if ((*I)->Status == pkgAcquire::Item::StatIdle)
		{
		    Transient = true;
		    // Failed = true;
		    continue;
		}

		string fetchError("Failed to fetch:\n");
		fetchError += (*I)->DescURI() + "  ";
		fetchError += (*I)->ErrorText;
		pk_backend_error_code(m_backend,
				      PK_ERROR_ENUM_PACKAGE_DOWNLOAD_FAILED,
				      fetchError.c_str());
// 		fprintf(stderr, "Failed to fetch %s  %s\n", (*I)->DescURI().c_str(),
// 			(*I)->ErrorText.c_str());
		Failed = true;
	    }

	    /* If we are in no download mode and missing files and there were
		'failures' then the user must specify -m. Furthermore, there
		is no such thing as a transient error in no-download mode! */
	    if (Transient == true &&
		_config->FindB("APT::Get::Download",true) == false)
	    {
		Transient = false;
		Failed = true;
	    }

// 	    if (_config->FindB("APT::Get::Download-Only",false) == true)
// 	    {
// 		if (Failed == true && _config->FindB("APT::Get::Fix-Missing",false) == false)
// 		    return _error->Error("Some files failed to download");
// 		c1out << "Download complete and in download only mode" << endl;
// 		return true;
// 	    }

	    if (Failed == true && _config->FindB("APT::Get::Fix-Missing",false) == false)
	    {
		return _error->Error("Unable to fetch some archives, maybe run apt-get update or try with --fix-missing?");
	    }

	    if (Transient == true && Failed == true)
		return _error->Error("--fix-missing and media swapping is not currently supported");

	    // Try to deal with missing package files
	    if (Failed == true && PM->FixMissing() == false)
	    {
		cerr << "Unable to correct missing packages." << endl;
		return _error->Error("Aborting install.");
	    }

	    _system->UnLock();
	    int status_fd = _config->FindI("APT::Status-Fd",-1);
	    pkgPackageManager::OrderResult Res = PM->DoInstall(status_fd);
	    if (Res == pkgPackageManager::Failed || _error->PendingError() == true)
		return false;
	    if (Res == pkgPackageManager::Completed)
		return true;

	    // Reload the fetcher object and loop again for media swapping
	    Fetcher.Shutdown();
	    if (PM->GetArchives(&Fetcher, &m_pkgSourceList, &Recs) == false)
		return false;

	    _system->Lock();
	}
}

bool aptcc::prepare_transaction(bool simulate, bool remove)
{
	bool WithLock = !simulate; // Check to see if we are just simulating,
			    //since for that no lock is needed

    //    CacheFile Cache;
	pkgCacheFile Cache;
	OpTextProgress Prog(*_config);
	// TODO do a loop here waiting for the lock if we need it..
	if (Cache.Open(Prog, WithLock) == false) {
		// failed to open cache, try checkDeps then..
		// || Cache.CheckDeps(CmdL.FileSize() != 1) == false
		return false;
	}

	// Enter the special broken fixing mode if the user specified arguments
	bool BrokenFix = false;
	if (Cache->BrokenCount() != 0) {
		BrokenFix = true;
	}

	unsigned int AutoMarkChanged = 0;
	unsigned int ExpectedInst = 0;
	unsigned int Packages = 0;
	pkgProblemResolver Fix(Cache);

	bool DefRemove = remove; // check if we are going to remove, otherwise is false

	// new scope for the ActionGroup
	{
		pkgDepCache::ActionGroup group(Cache);
		for (const char **I = CmdL.FileList + 1; *I != 0; I++)
		{
			// Duplicate the string
			unsigned int Length = strlen(*I);
			char S[300];
			if (Length >= sizeof(S))
			    continue;
			strcpy(S,*I);

			// See if we are removing and special indicators..
			bool Remove = DefRemove;
			char *VerTag = 0;
			bool VerIsRel = false;

		// Locate the package WE SHOULD ALREADY HAVE THE PACKAGE
    // 	    pkgCache::PkgIterator Pkg = Cache->FindPkg(S);
    // 	    Packages++;
    // 	    if (Pkg.end() == true)
    // 	    {
    // 		// Check if the name is a regex
    // 		const char *I;
    // 		for (I = S; *I != 0; I++)
    // 		if (*I == '?' || *I == '*' || *I == '|' ||
    // 		    *I == '[' || *I == '^' || *I == '$')
    // 		    break;
    // 		if (*I == 0)
    // 		return _error->Error(_("Couldn't find package %s"),S);
    //
    // 		// Regexs must always be confirmed
    // 		ExpectedInst += 1000;
    //
    // 		// Compile the regex pattern
    // 		regex_t Pattern;
    // 		int Res;
    // 		if ((Res = regcomp(&Pattern,S,REG_EXTENDED | REG_ICASE |
    // 				REG_NOSUB)) != 0)
    // 		{
    // 		char Error[300];
    // 		regerror(Res,&Pattern,Error,sizeof(Error));
    // 		return _error->Error(_("Regex compilation error - %s"),Error);
    // 		}
    //
    // 		// Run over the matches
    // 		bool Hit = false;
    // 		for (Pkg = Cache->PkgBegin(); Pkg.end() == false; Pkg++)
    // 		{
    // 		if (regexec(&Pattern,Pkg.Name(),0,0,0) != 0)
    // 		    continue;
    //
    // 		ioprintf(c1out,_("Note, selecting %s for regex '%s'\n"),
    // 			    Pkg.Name(),S);
    //
    // 		if (VerTag != 0)
    // 		    if (TryToChangeVer(Pkg,Cache,VerTag,VerIsRel) == false)
    // 			return false;
    //
    // 		Hit |= TryToInstall(Pkg,Cache,Fix,Remove,BrokenFix,
    // 				    ExpectedInst,false);
    // 		}
    // 		regfree(&Pattern);
    //
    // 		if (Hit == false)
    // 		return _error->Error(_("Couldn't find package %s"),S);
    // 	    }
    // 	    else
		{
    // 		if (VerTag != 0)
    // 		    if (TryToChangeVer(Pkg,Cache,VerTag,VerIsRel) == false)
    // 			return false;
			if (TryToInstall(Pkg,Cache,Fix,Remove,BrokenFix,ExpectedInst) == false) {
				return false;
			}

		}
	}

	/* If we are in the Broken fixing mode we do not attempt to fix the
	    problems. This is if the user invoked install without -f and gave
	    packages */
	if (BrokenFix == true && Cache->BrokenCount() != 0)
	{
	    c1out << _("You might want to run `apt-get -f install' to correct these:") << endl;
	    ShowBroken(c1out,Cache,false);

	    return _error->Error(_("Unmet dependencies. Try 'apt-get -f install' with no packages (or specify a solution)."));
	}

	// Call the scored problem resolver
	Fix.InstallProtect();
	if (Fix.Resolve(true) == false)
	    _error->Discard();

	// Now we check the state of the packages,
	if (Cache->BrokenCount() != 0)
	{
	    c1out <<
		_("Some packages could not be installed. This may mean that you have\n"
		"requested an impossible situation or if you are using the unstable\n"
		"distribution that some required packages have not yet been created\n"
		"or been moved out of Incoming.") << endl;
	    /*
	    if (Packages == 1)
	    {
		c1out << endl;
		c1out <<
		_("Since you only requested a single operation it is extremely likely that\n"
		    "the package is simply not installable and a bug report against\n"
		    "that package should be filed.") << endl;
	    }
	    */

	    c1out << _("The following information may help to resolve the situation:") << endl;
	    c1out << endl;
	    ShowBroken(c1out,Cache,false);
	    return _error->Error(_("Broken packages"));
	}
    }
    if (!DoAutomaticRemove(Cache))
	return false;

    /* Print out a list of packages that are going to be installed extra
	to what the user asked */
    if (Cache->InstCount() != ExpectedInst)
    {
	string List;
	string VersionsList;
	for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
	{
	    pkgCache::PkgIterator I(Cache,Cache.List[J]);
	    if ((*Cache)[I].Install() == false)
		continue;

	    const char **J;
	    for (J = CmdL.FileList + 1; *J != 0; J++)
		if (strcmp(*J,I.Name()) == 0)
		    break;

	    if (*J == 0) {
		List += string(I.Name()) + " ";
		VersionsList += string(Cache[I].CandVersion) + "\n";
	    }
	}

	ShowList(c1out,_("The following extra packages will be installed:"),List,VersionsList);
    }

    /* Print out a list of suggested and recommended packages */
    {
	string SuggestsList, RecommendsList, List;
	string SuggestsVersions, RecommendsVersions;
	for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
	{
	    pkgCache::PkgIterator Pkg(Cache,Cache.List[J]);

	    /* Just look at the ones we want to install */
	    if ((*Cache)[Pkg].Install() == false)
	    continue;

	    // get the recommends/suggests for the candidate ver
	    pkgCache::VerIterator CV = (*Cache)[Pkg].CandidateVerIter(*Cache);
	    for (pkgCache::DepIterator D = CV.DependsList(); D.end() == false; )
	    {
		pkgCache::DepIterator Start;
		pkgCache::DepIterator End;
		D.GlobOr(Start,End); // advances D

		// FIXME: we really should display a or-group as a or-group to the user
		//        the problem is that ShowList is incapable of doing this
		string RecommendsOrList,RecommendsOrVersions;
		string SuggestsOrList,SuggestsOrVersions;
		bool foundInstalledInOrGroup = false;
		for(;;)
		{
		/* Skip if package is  installed already, or is about to be */
		string target = string(Start.TargetPkg().Name()) + " ";

		if ((*Start.TargetPkg()).SelectedState == pkgCache::State::Install
		    || Cache[Start.TargetPkg()].Install())
		{
		    foundInstalledInOrGroup=true;
		    break;
		}

		/* Skip if we already saw it */
		if (int(SuggestsList.find(target)) != -1 || int(RecommendsList.find(target)) != -1)
		{
		    foundInstalledInOrGroup=true;
		    break;
		}

		// this is a dep on a virtual pkg, check if any package that provides it
		// should be installed
		if(Start.TargetPkg().ProvidesList() != 0)
		{
		    pkgCache::PrvIterator I = Start.TargetPkg().ProvidesList();
		    for (; I.end() == false; I++)
		    {
			pkgCache::PkgIterator Pkg = I.OwnerPkg();
			if (Cache[Pkg].CandidateVerIter(Cache) == I.OwnerVer() &&
			    Pkg.CurrentVer() != 0)
			    foundInstalledInOrGroup=true;
		    }
		}

		if (Start->Type == pkgCache::Dep::Suggests)
		{
		    SuggestsOrList += target;
		    SuggestsOrVersions += string(Cache[Start.TargetPkg()].CandVersion) + "\n";
		}

		if (Start->Type == pkgCache::Dep::Recommends)
		{
		    RecommendsOrList += target;
		    RecommendsOrVersions += string(Cache[Start.TargetPkg()].CandVersion) + "\n";
		}

		if (Start >= End)
		    break;
		Start++;
		}

		if(foundInstalledInOrGroup == false)
		{
		RecommendsList += RecommendsOrList;
		RecommendsVersions += RecommendsOrVersions;
		SuggestsList += SuggestsOrList;
		SuggestsVersions += SuggestsOrVersions;
		}

	    }
	}

	ShowList(c1out,_("Suggested packages:"),SuggestsList,SuggestsVersions);
	ShowList(c1out,_("Recommended packages:"),RecommendsList,RecommendsVersions);

    }

    // if nothing changed in the cache, but only the automark information
    // we write the StateFile here, otherwise it will be written in
    // cache.commit()
    if (AutoMarkChanged > 0 &&
	Cache->DelCount() == 0 && Cache->InstCount() == 0 &&
	Cache->BadCount() == 0 &&
	_config->FindB("APT::Get::Simulate",false) == false)
	Cache->writeStateFile(NULL);

    // See if we need to prompt
    if (Cache->InstCount() == ExpectedInst && Cache->DelCount() == 0)
	return InstallPackages(Cache,false,false);

    return InstallPackages(Cache,false);
}
