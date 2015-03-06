using System;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System.Threading.Tasks;
using System.Net.Http;
using System.Net;
using System.Linq;
using System.Web.Script.Serialization;

namespace Test
{

    public static class Settings
    {
        public static string password { get { return "55555"; } }
        public static string[] FilesExpected = { "INDEX.GZ", "SETTINGS.TXT", "CACHE.APP", "SESSION.TXT", "LOG.TXT" };
        public static int SettingLength = 9;
    }

    [TestClass]
    public class GetFiles : BaseClass
    {
        
        public class SysAdmin {
            //{"DeviceTime":"2015-03-01 22:46:46","MinRam":"385b","MaxRam":"419b","RunningSince":"2015-03-01 13:27:24","Settings":"555550211"}
            public string DeviceTime { get; set; }
            public string MinRam { get; set; }
            public string MaxRam { get; set; }
            public string RunningSince { get; set; }
            public string Settings { get; set; }
        }

        //[TestMethod]
        //[TestCategory("INTEGRATION")]
        //[TestProperty("Type", "GET")]
        //public async Task Options_GET()
        //{
            
        //    HttpResponseMessage response = await _client.GetAsync("/");
        //    response.EnsureSuccessStatusCode();
        //    Assert.AreEqual(HttpStatusCode.OK, response.StatusCode);

        //    Assert.IsTrue(response.Content.Headers.ContentEncoding.Contains("gzip"));
        //    Assert.IsTrue(response.Content.Headers.ContentType.MediaType.Equals("text/html"));

        //    Assert.IsTrue(response.Headers.Single(h => h.Key == "Access-Control-Allow-Origin").Value.FirstOrDefault().Equals("*"));
        //    Assert.IsTrue(response.Headers.Single(h => h.Key == "Access-Control-Allow-Headers").Value.FirstOrDefault().Equals("PP"));

        //    string responseBody = await response.Content.ReadAsStringAsync();
        //    //List<Face.Types.Response.AddressResponse> tweb = ServiceStack.Text.JsonSerializer.DeserializeFromString<List<Face.Types.Response.AddressResponse>>(responseBody);

        //    Assert.IsTrue(responseBody.Contains("index.htm"));

        //}

        [TestMethod]
        [TestCategory("INTEGRATION")]
        [TestProperty("Type", "GET")]
        public async Task Html_GET()
        {
            newClient();
            addGzipHeader();
            HttpResponseMessage response = await _client.GetAsync("/");
            //response.EnsureSuccessStatusCode();
            Assert.AreEqual(HttpStatusCode.OK, response.StatusCode, "response is not 200");

            Assert.IsTrue(response.Content.Headers.ContentEncoding.Contains("gzip"), "response encoding is not gzip");
            Assert.IsTrue(response.Content.Headers.ContentType.MediaType.Equals("text/html"), "response type is not html");

            Assert.IsTrue(response.Headers.Single(h => h.Key == "Access-Control-Allow-Origin").Value.FirstOrDefault().Equals("*"), "Missing Allow Origin");
            Assert.IsTrue(response.Headers.Single(h => h.Key == "Access-Control-Allow-Headers").Value.FirstOrDefault().Equals("PP"), "Missing Allow Headers");

            string responseBody = await response.Content.ReadAsStringAsync();

            Assert.IsTrue(responseBody.Contains("index.htm"), "Body did not return index");

        }

        [TestMethod]
        [TestCategory("INTEGRATION")]
        [TestProperty("Type", "GET")]
        public async Task Html_NoGzip_GET()
        {
            newClient();            
            HttpResponseMessage response = await _client.GetAsync("/");
            Assert.AreEqual(HttpStatusCode.Unauthorized, response.StatusCode, "response code is not 401");

        }

        [TestMethod]
        [TestCategory("INTEGRATION")]
        [TestProperty("Type", "GET")]
        public async Task Cache_GET()
        {

            newClient();
            HttpResponseMessage response = await _client.GetAsync("/cache.app");
           Assert.AreEqual(HttpStatusCode.OK, response.StatusCode, "response is not 200");

           Assert.IsTrue(response.Content.Headers.ContentType.MediaType.Equals("text/cache-manifest"), "response type is not html");;

            Assert.IsTrue(response.Headers.Single(h => h.Key == "Access-Control-Allow-Origin").Value.FirstOrDefault().Equals("*"), "Missing Allow Origin");
            Assert.IsTrue(response.Headers.Single(h => h.Key == "Access-Control-Allow-Headers").Value.FirstOrDefault().Equals("PP"), "Missing Allow Headers");

            string responseBody = await response.Content.ReadAsStringAsync();

            Assert.IsTrue(responseBody.Contains("CACHE MANIFEST"), "Not manifest file!");
            Assert.IsTrue(responseBody.Contains("CACHE:"), "Manifest file missing cache section!");

        }

        [TestMethod]
        [TestCategory("INTEGRATION")]
        [TestProperty("Type", "GET")]
        public async Task IsAdmin_GET()
        {
            newClient();
            HttpResponseMessage response = await _client.GetAsync("/admin?U=" + Settings.password);
            Assert.AreEqual(HttpStatusCode.OK, response.StatusCode, "response is not 200");

            Assert.IsTrue(response.Content.Headers.ContentType.MediaType.Equals("application/json"), "response type is not json"); ;

            Assert.IsTrue(response.Headers.Single(h => h.Key == "Access-Control-Allow-Origin").Value.FirstOrDefault().Equals("*"), "Missing Allow Origin");
            Assert.IsTrue(response.Headers.Single(h => h.Key == "Access-Control-Allow-Headers").Value.FirstOrDefault().Equals("PP"), "Missing Allow Headers");


        }

        [TestMethod]
        [TestCategory("INTEGRATION")]
        [TestProperty("Type", "GET")]
        public async Task IsNotAdmin_GET()
        {
            newClient();
            HttpResponseMessage response = await _client.GetAsync("/admin?U=00000");
            Assert.AreEqual(HttpStatusCode.Unauthorized, response.StatusCode, "response code is not 401");
            Assert.IsTrue(response.Content.Headers.ContentType.MediaType.Equals("application/json"), "response type is not json"); ;

            Assert.IsTrue(response.Headers.Single(h => h.Key == "Access-Control-Allow-Origin").Value.FirstOrDefault().Equals("*"), "Missing Allow Origin");
            Assert.IsTrue(response.Headers.Single(h => h.Key == "Access-Control-Allow-Headers").Value.FirstOrDefault().Equals("PP"), "Missing Allow Headers");
        }

        [TestMethod]
        [TestCategory("INTEGRATION")]
        [TestProperty("Type", "GET")]
        public async Task SystemAdmin_GET()
        {
            newClient();
            _client.DefaultRequestHeaders.Add("PP", Settings.password);

            HttpResponseMessage response = await _client.GetAsync("/sysadmin");
            //response.EnsureSuccessStatusCode();
            Assert.AreEqual(HttpStatusCode.OK, response.StatusCode, "response is not 200");
            Assert.IsTrue(response.Content.Headers.ContentType.MediaType.Equals("application/json"), "response type is not json"); ;

            Assert.IsTrue(response.Headers.Single(h => h.Key == "Access-Control-Allow-Origin").Value.FirstOrDefault().Equals("*"), "Missing Allow Origin");
            Assert.IsTrue(response.Headers.Single(h => h.Key == "Access-Control-Allow-Headers").Value.FirstOrDefault().Equals("PP"), "Missing Allow Headers");
            
            string responseBody = await response.Content.ReadAsStringAsync();

            SysAdmin res = new JavaScriptSerializer().Deserialize<SysAdmin>(responseBody);

            Assert.IsTrue(res.DeviceTime.Length == 19);
            Assert.IsTrue(res.RunningSince.Length == 19);
            Assert.IsTrue(res.MinRam.Length == 4);
            Assert.IsTrue(res.MaxRam.Length == 4);
            Assert.IsTrue(res.Settings.Length == 9);

        }

        [TestMethod]
        [TestCategory("INTEGRATION")]
        [TestProperty("Type", "GET")]
        public async Task GetFilesAdmin_GET()
        {
            newClient();
            _client.DefaultRequestHeaders.Add("PP", Settings.password);

            HttpResponseMessage response = await _client.GetAsync("/files");
            //response.EnsureSuccessStatusCode();
            Assert.AreEqual(HttpStatusCode.OK, response.StatusCode, "response is not 200");
            Assert.IsTrue(response.Content.Headers.ContentType.MediaType.Equals("application/json"), "response type is not json"); ;

            Assert.IsTrue(response.Headers.Single(h => h.Key == "Access-Control-Allow-Origin").Value.FirstOrDefault().Equals("*"), "Missing Allow Origin");
            Assert.IsTrue(response.Headers.Single(h => h.Key == "Access-Control-Allow-Headers").Value.FirstOrDefault().Equals("PP"), "Missing Allow Headers");

            string responseBody = await response.Content.ReadAsStringAsync();

            foreach (string s in Settings.FilesExpected)
            {
                Assert.IsTrue(responseBody.Contains(s),  s + " is not there!");

            }            
        }


        [TestMethod]
        [TestCategory("INTEGRATION")]
        [TestProperty("Type", "GET")]
        public async Task GetFileSettingsAdmin_GET()
        {
            newClient();
            _client.DefaultRequestHeaders.Add("PP", Settings.password);

            HttpResponseMessage response = await _client.GetAsync("/file/SETTINGS.TXT");
            //response.EnsureSuccessStatusCode();
            Assert.AreEqual(HttpStatusCode.OK, response.StatusCode, "response is not 200");
            Assert.IsTrue(response.Content.Headers.ContentType.MediaType.Equals("text/html"), "response type is not html"); ;

            Assert.IsTrue(response.Headers.Single(h => h.Key == "Access-Control-Allow-Origin").Value.FirstOrDefault().Equals("*"), "Missing Allow Origin");
            Assert.IsTrue(response.Headers.Single(h => h.Key == "Access-Control-Allow-Headers").Value.FirstOrDefault().Equals("PP"), "Missing Allow Headers");

            string responseBody = await response.Content.ReadAsStringAsync();


            Assert.IsTrue(responseBody.Trim().Length == Settings.SettingLength, "Settings length not matching!");
        }

    }
}
