#include <map>
#include <string>

#include "CloudiskServer.h"
#include "CryptoUtil.h"
#include <wfrest/PathUtil.h>
#include <workflow/MySQLResult.h>
#include <workflow/Workflow.h>
#include "workflow/WFFacilities.h"
#include "Sign.srpc.h"


using namespace wfrest;
using namespace srpc;

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
	wait_group.done();
}

void CloudiskServer::register_modules()
{
    // 设置静态资源的路由
    register_static_resources_module();
    register_signup_module();
    register_signin_module();
    register_userinfo_module();
    register_fileupload_module();
    register_filelist_module();
    register_filedownload_module();
}

void CloudiskServer::register_static_resources_module()
{
    m_server.GET("/user/signup", [](const HttpReq *, HttpResp *resp) { resp->File("static/view/signup.html"); });

    m_server.GET("/static/view/signin.html",
                 [](const HttpReq *, HttpResp *resp) { resp->File("static/view/signin.html"); });

    m_server.GET("/static/view/home.html",
                 [](const HttpReq *, HttpResp *resp) { resp->File("static/view/home.html"); });

    m_server.GET("/static/js/auth.js", [](const HttpReq *, HttpResp *resp) { resp->File("static/js/auth.js"); });

    m_server.GET("/static/img/avatar.jpeg",
                 [](const HttpReq *, HttpResp *resp) { resp->File("static/img/avatar.jpeg"); });

    m_server.GET("/file/upload", [](const HttpReq *, HttpResp *resp) { resp->File("static/view/index.html"); });

    m_server.Static("/file/upload_files", "static/view/upload_files");
}

// 注册---------------------------------------------------------------------------------------------------------------------------
void CloudiskServer::register_signup_module()
{
    m_server.POST("/user/signup", [](const HttpReq *req, HttpResp *resp) {
        if (req->content_type() != APPLICATION_URLENCODED)
        {
            resp->String("unsupported type");
            return;
        }

        std::map<std::string, std::string> &params = req->form_kv();

        std::string username = params["username"];
        std::string password = params["password"];

        if (username.empty() || password.empty())
        {
            resp->String("username or password is empty");
            return;
        }

        std::string salt = CryptoUtil::generate_salt();
        std::string hashed_password = CryptoUtil::hash_password(password, salt);

        // 写入MySQL
        std::string sql = "INSERT INTO tbl_user (username, password, salt) VALUES ('" + username + "', '" +
                          hashed_password + "', '" + salt + "')";
        std::string mysql_url = "mysql://root:123@localhost:3306/webnet";
        resp->MySQL(mysql_url, sql, [resp](protocol::MySQLResultCursor *result) {
            if (result->get_cursor_status())
            {
                resp->String("SUCCESS");
            }
            else
            {
                resp->String("signup failed");
            }
        });
    });
}

// 登录模块---------------------------------------------------------------------------------------------------------------
void CloudiskServer::register_signin_module()
{
    m_server.POST("/user/signin", [](const HttpReq *req, HttpResp *resp) {
        if (req->content_type() != APPLICATION_URLENCODED)
        {
            resp->String("unsupported type");
            return;
        }

        std::map<std::string, std::string> &params = req->form_kv();

        std::string username = params["username"];
        std::string password = params["password"];

        if (username.empty() || password.empty())
        {
            resp->String("username or password is empty");
            return;
        }

        // 从数据库中查询用户信息
        std::string sql = "SELECT password, salt, tomb FROM tbl_user WHERE username = '" + username + "'";
        std::string mysql_url = "mysql://root:123@localhost:3306/webnet";
        resp->MySQL(mysql_url, sql, [resp, username, password](protocol::MySQLResultCursor *result) {
            if (result->get_cursor_status() == MYSQL_STATUS_GET_RESULT)
            {
                if (result->get_rows_count() == 0)
                {
                    resp->String("user not found");
                    return;
                }

                std::string db_password;
                std::string db_salt;
                std::vector<protocol::MySQLCell> row;
                if (result->fetch_row(row))
                {
                    if (row[2].as_int() != 0)
                    {
                        resp->String("user has been deleted");
                        return;
                    }
                    db_password = row[0].as_string();
                    db_salt = row[1].as_string();
                }
                else
                {
                    resp->String("failed to fetch user data");
                    return;
                }

                // 验证密码
                std::string hashed_password = CryptoUtil::hash_password(password, db_salt);

                if (hashed_password == db_password)
                {
                    std::string token = CryptoUtil::generate_token(username);
                    Json json;
                    json["Username"] = username;
                    json["Token"] = token;
                    json["Location"] = "/static/view/home.html";
                    Object_S data;
                    data.push_back("data", json);
                    resp->Json(data);
                }
                else
                {
                    resp->String("password is incorrect");
                }
            }
            else
            {
                resp->String("signin failed");
            }
        });
    });
}

void CloudiskServer::register_userinfo_module()
{
    m_server.GET("/user/info", [](const HttpReq *req, HttpResp *resp) {
        if (!req->has_query("token") || !req->has_query("username"))
        {
            resp->String("necessary parameter token is missing");
            return;
        }

        std::string token = req->query("token");
        std::string username = req->query("username");
        if (token.empty())
        {
            resp->String("token is empty");
            return;
        }
        if (username.empty())
        {
            resp->String("username is empty");
            return;
        }

        if (!CryptoUtil::verify_token(token, username))
        {
            resp->String("invalid token");
            return;
        }

        // 从数据库中查询用户信息
        std::string sql = "SELECT username, created_at FROM tbl_user WHERE username = '" + username + "'";
        std::string mysql_url = "mysql://root:123@localhost:3306/webnet";
        resp->MySQL(mysql_url, sql, [resp](protocol::MySQLResultCursor *result) {
            if (result->get_cursor_status() == MYSQL_STATUS_GET_RESULT)
            {
                if (result->get_rows_count() == 0)
                {
                    resp->String("user not found");
                    return;
                }

                std::vector<protocol::MySQLCell> row;
                if (result->fetch_row(row))
                {
                    Json json;
                    json["username"] = row[0].as_string();
                    json["signupat"] = row[1].as_string();
                    resp->Json(json);
                }
                else
                {
                    resp->String("failed to fetch user data");
                }
            }
            else
            {
                resp->String("userinfo query failed");
            }
        });
    });
}
void CloudiskServer::register_fileupload_module()
{
    m_server.POST("/file/upload", [](const HttpReq *req, HttpResp *resp, SeriesWork *series_work) {
        // username和token参数校验
        if (!req->has_query("token") || !req->has_query("username"))
        {
            resp->String("necessary parameter token is missing");
            return;
        }
        std::string token = req->query("token");
        std::string username = req->query("username");
        if (token.empty())
        {
            resp->String("token is empty");
            return;
        }
        if (username.empty())
        {
            resp->String("username is empty");
            return;
        }
        if (!CryptoUtil::verify_token(token, username))
        {
            resp->String("invalid token");
            return;
        }

        if (req->content_type() != MULTIPART_FORM_DATA)
        {
            resp->set_status(HttpStatusBadRequest);
            return;
        }

        /*
         * https://developer.mozilla.org/en-US/docs/Web/HTTP/Methods/POST
         * <name, <filename, body>>
         * using Form = std::map<std::string, std::pair<std::string, std::string>>;
         */

        // 获取uid
        std::string sql = "SELECT id FROM tbl_user WHERE username = '" + username + "'";
        std::string mysql_url = "mysql://root:123@localhost:3306/webnet";
        resp->MySQL(mysql_url, sql, [resp, req,username](protocol::MySQLResultCursor *result) {
            if (result->get_cursor_status() != MYSQL_STATUS_GET_RESULT)
            {
                resp->String("failed to get user id");
                return;
            }

            if (result->get_rows_count() == 0)
            {
                resp->String("user not found");
                return;
            }

            std::vector<protocol::MySQLCell> row;
            if (!result->fetch_row(row))
            {
                resp->String("failed to fetch user id");
                return;
            }
            int uid = row[0].as_int();
            printf("uid: %d\n", uid);

            const Form &form = req->form();
            for (const auto &[_key, file] : form)
            {
                const auto &[filename, content] = file;
                int len = static_cast<std::string>(content).length();
                std::string len_str = std::to_string(len);
                resp->Save(PathUtil::base(filename), std::move(content),
                           "file of " + username + " : " + filename + " save to cloudisk\n");
                std::string hashcode = CryptoUtil::hash_password(len_str, PathUtil::base(filename));

                //printf("filename: %s, len: %d, hashcode: %s\n", filename.c_str(), len, hashcode.c_str());

                // 将文件信息写入数据库
                std::string sql ="INSERT INTO tbl_file (uid, hashcode, filename, size, created_at, last_update) VALUES (" + std::to_string(uid) + ",'" + hashcode + "','" + PathUtil::base(filename) + "'," + len_str + ",NOW(),NOW())";
                std::string mysql_url = "mysql://root:123@localhost:3306/webnet";
                resp->MySQL(mysql_url, sql, [resp](protocol::MySQLResultCursor *result2) {
                    //printf("%d\n", result2->get_cursor_status());
                    if (result2->get_cursor_status() == MYSQL_STATUS_OK)
                    {
                        if (result2->get_affected_rows() > 0)
                        {
                            resp->String("success write file info to mysql\n");
                        }
                        else
                        {
                            resp->String("fail write file info to mysql_1\n");
                        }
                    }
                    else
                    {
                        resp->String("fail write file info to mysql_2\n");
                    }
                });
            }
        });
    });
}

void CloudiskServer::register_filelist_module()
{
    m_server.POST("/file/query", [](const HttpReq *req, HttpResp *resp, SeriesWork *series_work) {
        // username和token参数校验
        if (!req->has_query("token") || !req->has_query("username"))
        {
            resp->String("necessary parameter token is missing");
            return;
        }
        std::string token = req->query("token");
        std::string username = req->query("username");
        if (token.empty())
        {
            resp->String("token is empty");
            return;
        }
        if (username.empty())
        {
            resp->String("username is empty");
            return;
        }
        if (!CryptoUtil::verify_token(token, username))
        {
            resp->String("invalid token");
            return;
        }

        // 获取uid
        std::string sql = "SELECT id FROM tbl_user WHERE username = '" + username + "'";
        std::string mysql_url = "mysql://root:123@localhost:3306/webnet";
        resp->MySQL(mysql_url, sql, [resp, req](protocol::MySQLResultCursor *result) {
            if (result->get_cursor_status() != MYSQL_STATUS_GET_RESULT)
            {
                resp->String("failed to get user id");
                return;
            }

            if (result->get_rows_count() == 0)
            {
                resp->String("user not found");
                return;
            }

            std::vector<protocol::MySQLCell> row;
            if (!result->fetch_row(row))
            {
                resp->String("failed to fetch user id");
                return;
            }

            int uid = row[0].as_int();
            printf("uid: %d\n", uid);

            // 从数据库中查询文件列表
            std::map<std::string, std::string> &params = req->form_kv();
            std::string limit = params["limit"];
            if (limit.empty())
            {
                limit = "10";
            }

            std::string file_sql =
                "SELECT hashcode, filename, size, created_at , last_update FROM tbl_file WHERE uid = " + std::to_string(uid) + " ORDER BY created_at DESC LIMIT " + limit; // 添加限制条件
            std::string mysql_url = "mysql://root:123@localhost:3306/webnet";
            resp->MySQL(mysql_url, file_sql, [resp](protocol::MySQLResultCursor *file_result) {
                printf("%d\n", file_result->get_cursor_status());
                if (file_result->get_cursor_status() != MYSQL_STATUS_GET_RESULT)
                {
                    resp->String("failed to get file list");
                    return;
                }

                Array_S array;
                std::vector<protocol::MySQLCell> row;
                while (file_result->fetch_row(row))
                {
                    Json file_json;
                    file_json["hashnode"] = row[0].as_string();
                    file_json["filename"] = row[1].as_string();
                    file_json["size"] = row[2].as_int();
                    file_json["created_at"] = row[3].as_string();
                    file_json["last_update"] = row[4].as_string();
                    array.push_back(file_json);
                }
                resp->Json(array);
            });
        });
    });
}



void CloudiskServer::register_filedownload_module()
{
    m_server.GET("/file/download", [](const HttpReq *req, HttpResp *resp) {
        // username和token参数校验
        if (!req->has_query("token") || !req->has_query("username"))
        {
            resp->String("necessary parameter token is missing");
            return;
        }
        std::string token = req->query("token");
        std::string username = req->query("username");
        if (token.empty())
        {
            resp->String("token is empty");
            return;
        }
        if (username.empty())
        {
            resp->String("username is empty");
            return;
        }
        if (!CryptoUtil::verify_token(token, username))
        {
            resp->String("invalid token");
            return;
        }

        if(!req->has_query("filename")){
            resp->String("necessary parameter filename is missing");
        }
        std::string filename = req->query("filename");
        if (filename.empty())
        {
            resp->String("filename is empty");
            return;
        }

        // resp->add_header_pair("Content-Type","application/octet-stream");
        // resp->set_header_pair("Content-Type","application/octet-stream");
        resp->set_header_pair("Content-Disposition", "attachment; filename=\"" + PathUtil::base(filename) + "\"");
        resp->File(filename);


    });
}
