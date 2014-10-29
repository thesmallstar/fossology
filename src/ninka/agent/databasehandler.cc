/*
 * Copyright (C) 2014, Siemens AG
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "databasehandler.hpp"
#include "libfossUtils.hpp"

NinkaDatabaseHandler::NinkaDatabaseHandler(DbManager dbManager) :
  fo::AgentDatabaseHandler(dbManager)
{
}

// TODO: see function queryFileIdsForUpload() from src/lib/c/libfossagent.c
vector<unsigned long> NinkaDatabaseHandler::queryFileIdsForUpload(int uploadId)
{
  QueryResult queryResult = dbManager.execPrepared(
    fo_dbManager_PrepareStamement(
      dbManager.getStruct_dbManager(),
      "queryFileIdsForUpload",
      "SELECT DISTINCT(pfile_fk) FROM uploadtree WHERE upload_fk = $1 AND (ufile_mode&x'3C000000'::int) = 0",
      int
    ),
    uploadId
  );

  return queryResult.getSimpleResults<unsigned long>(0, fo::stringToUnsignedLong);
}

// TODO: see function saveToDb() from src/monk/agent/database.c
bool NinkaDatabaseHandler::saveLicenseMatch(int agentId, long pFileId, long licenseId, unsigned percentMatch)
{
  return dbManager.execPrepared(
    fo_dbManager_PrepareStamement(
      dbManager.getStruct_dbManager(),
      "saveLicenseMatch",
      "INSERT INTO license_file (agent_fk, pfile_fk, rf_fk, rf_match_pct) VALUES ($1, $2, $3, $4)",
      int, long, long, unsigned
    ),
    agentId,
    pFileId,
    licenseId,
    percentMatch
  );
}

unsigned long NinkaDatabaseHandler::selectOrInsertLicenseIdForName(string rfShortName)
{
  QueryResult queryResult = dbManager.execPrepared(
    fo_dbManager_PrepareStamement(
      dbManager.getStruct_dbManager(),
      "selectOrInsertLicenseIdForName",
      "WITH "
      "selectExisting AS ("
        "SELECT rf_pk FROM license_ref WHERE rf_shortname = $1 LIMIT 1"
      "),"
      "insertNew AS ("
        "INSERT INTO license_ref(rf_shortname, rf_text, rf_detector_type)"
        " SELECT $1, $2, $3"
        " WHERE NOT EXISTS(SELECT * FROM selectExisting)"
        " RETURNING rf_pk"
      ") "

      "SELECT rf_pk FROM insertNew "
      "UNION "
      "SELECT rf_pk FROM selectExisting",
      char*, char*, int
    ),
    rfShortName.c_str(),
    "License by Ninka.",
    3
  );

  if(!queryResult)
    return 0;

  if (queryResult.getRowCount() > 0) {
    return queryResult.getSimpleResults(0, fo::stringToUnsignedLong)[0];
  }

  return 0;
}

NinkaDatabaseHandler NinkaDatabaseHandler::spawn() const
{
  DbManager spawnedDbMan(dbManager.spawn());
  return NinkaDatabaseHandler(spawnedDbMan);
}

unsigned long NinkaDatabaseHandler::getLicenseIdForName(string const & rfShortName)
{
  std::unordered_map<string,long>::const_iterator findIterator = licenseRefCache.find(rfShortName);
  if (findIterator != licenseRefCache.end())
  {
    return findIterator->second;
  }
  else
  {
    unsigned long licenseId = selectOrInsertLicenseIdForName(rfShortName);
    if (licenseId > 0)
    {
      licenseRefCache.insert(std::make_pair(rfShortName, licenseId));
      return licenseId;
    }
  }

  return 0;
}