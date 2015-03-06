using System;
//using System.Collections.Generic;
//using System.Linq;
//using System.Text;
//using System.Threading.Tasks;

namespace Test
{
    using System.Configuration;
    using System.Net.Http;
    using System.Net.Http.Headers;

    public class BaseClass
    {

        protected HttpClient _client;

        public BaseClass() 
        {

        }

        public void newClient()
        {
            HttpClientHandler handler = new HttpClientHandler();
            handler.UseDefaultCredentials = true;
            _client = new HttpClient(handler);
            _client.DefaultRequestHeaders.Accept.Add(new MediaTypeWithQualityHeaderValue("application/json"));
            _client.BaseAddress = new Uri(ConfigurationManager.AppSettings["TestBaseUrl"]);

        }

        public void addGzipHeader()
        {
            //Accept-Encoding: gzip
            _client.DefaultRequestHeaders.TryAddWithoutValidation("Accept-Encoding", "gzip, deflate");
            //_client.DefaultRequestHeaders.AcceptEncoding.Add("gzip");
           
        }

    }
}
